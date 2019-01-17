#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sqlite3.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h>
#include <sys/sysinfo.h>

#define TRAFFIC_DB_NAME                     "/usr/data/traffic.db"
#define TRAFFIC_PROC_NAME                   "/proc/net/conn_stats" 
#define MAX_BUFFER_LEN                      8192
#define BUSY_TIMEOUT                        (10*1000)

#define  TRAFFIC_FTP                        0
#define  TRAFFIC_WEB                        1
#define  TRAFFIC_IM                         2
#define  TRAFFIC_EMAIL                      3
#define  TRAFFIC_P2P                        4
#define  TRAFFIC_DB                         5
#define  TRAFFIC_WEBPROXY                   6
#define  TRAFFIC_RL                         7
#define  TRAFFIC_MEDIA                      8
#define  TRAFFIC_VOIP                       9
#define  TRAFFIC_STOCK                      10
#define  TRAFFIC_GAME                       11 
#define  TRAFFIC_OTHER                      12 
#define  TRAFFIC_TOTAL                      13 
#define  TRAFFIC_MAX                        14 

#define DB_UPDATE_STEP_10SEC                10
#define DB_UPDATE_STEP_1MIN                 60
#define DB_UPDATE_STEP_20MIN                (20*60)

#define TRAFFIC_TOTAL_10M_MAX_COUNT         61
#define TRAFFIC_TOTAL_1H_MAX_COUNT          61
#define TRAFFIC_TOTAL_1D_MAX_COUNT          73 

int g_syslog_level = LOG_ERR;

char *g_syslog_levels[] = { 
    "EMERG",
    "ALERT",
    "CRTIC",
    "ERROR",
    "WARNG",
    "NOTIC",
    "INFFO",
    "DEBUG"
};

#define SYSLOG_R(priority, X, Y...) {                                           \
        if (priority <= g_syslog_level) {                                       \
            char _id[20] = {0};                                                 \
            snprintf(_id, 20, "[%s] %s", g_syslog_levels[priority], "STATS");   \
            openlog(_id, LOG_NDELAY, LOG_USER);                                 \
            syslog(priority, X, ##Y);                                           \
            closelog();                                                         \
        }                                                                       \
    }

enum traffic_table_type 
{
    TRAFFIC_TABLE_TOTAL_NONE            = 0,
    TRAFFIC_TABLE_TOTAL_10M             = 1,
    TRAFFIC_TABLE_TOTAL_1H              = 2,
    TRAFFIC_TABLE_TOTAL_1D              = 3,
    TRAFFIC_TABLE_TOTAL_REAL_10M        = 4,
    TRAFFIC_TABLE_TOTAL_REAL_1H         = 5,
    TRAFFIC_TABLE_TOTAL_REAL_1D         = 6,
    TRAFFIC_TABLE_UP_10M                = 7,
    TRAFFIC_TABLE_UP_1H                 = 8,
    TRAFFIC_TABLE_UP_1D                 = 9,
    TRAFFIC_TABLE_UP_REAL_10M           = 10,
    TRAFFIC_TABLE_UP_REAL_1H            = 11,
    TRAFFIC_TABLE_UP_REAL_1D            = 12,
    TRAFFIC_TABLE_DOWN_10M              = 13,
    TRAFFIC_TABLE_DOWN_1H               = 14,
    TRAFFIC_TABLE_DOWN_1D               = 15,
    TRAFFIC_TABLE_DOWN_REAL_10M         = 16,
    TRAFFIC_TABLE_DOWN_REAL_1H          = 17,
    TRAFFIC_TABLE_DOWN_REAL_1D          = 18,
    TRAFFIC_TABLE_TOTAL_MAX             = 19,
};

struct traffic_info {
    uint64_t up;
    uint64_t down;
    uint64_t total;
};

static sqlite3 *g_traffic_db = NULL;
struct traffic_info traffic_new[TRAFFIC_MAX];
struct traffic_info traffic_old[TRAFFIC_TABLE_TOTAL_MAX][TRAFFIC_MAX];
int traffic_old_init_flag[TRAFFIC_TABLE_TOTAL_MAX];

static int g_table_count[TRAFFIC_TABLE_TOTAL_MAX];

char *g_table_name[TRAFFIC_TABLE_TOTAL_MAX] = 
{
    "none",
    "total_traffic_10m",
    "total_traffic_1h",
    "total_traffic_1d",
    "total_real_traffic_10m",
    "total_real_traffic_1h",
    "total_real_traffic_1d",
    "up_traffic_10m",
    "up_traffic_1h",
    "up_traffic_1d",
    "up_real_traffic_10m",
    "up_real_traffic_1h",
    "up_real_traffic_1d",
    "down_traffic_10m",
    "down_traffic_1h",
    "down_traffic_1d",
    "down_real_traffic_10m",
    "down_real_traffic_1h",
    "down_real_traffic_1d",
};

static int db_create(char *filename)
{
    int ret = -1;
    char *errmsg = NULL;
    char sql[512] = "";
    int i = 0;

    sqlite3_open(filename, &g_traffic_db);
    if (g_traffic_db == NULL)
    {
        SYSLOG_R(LOG_ERR,"%s:%d: open sqlite dbfile failed:%s\n", __func__, __LINE__, errmsg);
        goto exit;
    }

    sqlite3_busy_timeout(g_traffic_db, BUSY_TIMEOUT);

    for(i = TRAFFIC_TABLE_TOTAL_10M; i < TRAFFIC_TABLE_TOTAL_MAX; i++)
    {
        snprintf(sql, 512, "create table %s(ftp double, web double,"
                "im double, email double, p2p double, db double, webproxy double, rl double,"
                "media double, voip double, stock double, game double, other double,"
                "total double, timestamp Integer)", g_table_name[i]);
        if (sqlite3_exec(g_traffic_db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
        {
            SYSLOG_R(LOG_ERR,"%s:%d: create table failed:%s\n", __func__, __LINE__, errmsg);
            sqlite3_free(errmsg);
            goto exit;
        }
        sqlite3_free(errmsg);
    }

    ret = 0;

exit:
    return ret;
}

//sqlite3_exec在查询表的时候，每查到一条记录，就会调用一次回调函数
static int traffic_table_count_callback(void *parg, int narg, char **azarg, char **azcol)
{
    int type = *((int *)parg);

    if(type <= TRAFFIC_TABLE_TOTAL_NONE || type >= TRAFFIC_TABLE_TOTAL_MAX)
        return -1;

    g_table_count[type] = azarg[0] ? atol(azarg[0]) : 0; 

    return 0;
}


static int get_table_count(int type)
{
    int ret = -1;
    char *errmsg = NULL;
    char sql[512] = "";

    if(type <= TRAFFIC_TABLE_TOTAL_NONE || type >= TRAFFIC_TABLE_TOTAL_MAX)
        return -1;

    snprintf(sql, 512, "select count(*) from %s", g_table_name[type]);

    ret = sqlite3_exec(g_traffic_db, sql, traffic_table_count_callback, &type, &errmsg);
    
    if (ret != SQLITE_OK) {
        SYSLOG_R(LOG_ERR,"%s:%d: %s\n", __func__, __LINE__, errmsg);
    }
    sqlite3_free(errmsg);
        
    ret = (ret == SQLITE_OK) ? 0 : -1;

    return ret;
}

static int traffic_table_count_init(void)
{
    int i = 0;

    for(i = 1; i < TRAFFIC_TABLE_TOTAL_MAX; i++)
        get_table_count(i);

    return 0;
}

int database_init(void)
{
    int ret = -1;
    char *errmsg = NULL;

    if(unlink(TRAFFIC_DB_NAME) != 0)
        SYSLOG_R(LOG_ERR,"%s:%d: Delete old database error:%s\n", __func__, __LINE__, errmsg);

    if(access(TRAFFIC_DB_NAME, F_OK) != 0) {
        SYSLOG_R(LOG_DEBUG,"%s:%d: create database file and table\n", __func__, __LINE__);
        ret = db_create(TRAFFIC_DB_NAME);
        if(ret != 0) 
            goto exit;
    }
    else {
        sqlite3_open(TRAFFIC_DB_NAME, &g_traffic_db);
        if (g_traffic_db == NULL)
        {
            SYSLOG_R(LOG_ERR,"%s:%d: open sqlite dbfile failed:%s\n", __func__, __LINE__, errmsg);
            goto exit;
        }

        sqlite3_busy_timeout(g_traffic_db, BUSY_TIMEOUT);
        SYSLOG_R(LOG_DEBUG,"%s:%d: database is exist\n", __func__, __LINE__);
    }

    ret = traffic_table_count_init();
    if (ret != 0)
        goto exit;

exit:
    return ret;
}

int get_traffic_data(void)
{
    int stats_fd = -1;
    int ret = -1;
    int traffic_id = 0;
    char buffer[MAX_BUFFER_LEN] = "";
    char *pend = NULL;
    char *pbegin = NULL;
    char  name[20] = "";
    unsigned int conn_tcp = 0;
    unsigned int conn_udp = 0;
    unsigned long long up = 0;
    unsigned long long down = 0;
    unsigned long long total_up = 0;
    unsigned long long total_down = 0;

    stats_fd = open(TRAFFIC_PROC_NAME, O_RDONLY);
    if(stats_fd < 0) {
        SYSLOG_R(LOG_ERR,"Cannot open %s : %s\n", TRAFFIC_PROC_NAME, strerror(errno));
        goto exit;
    }

    ret = read(stats_fd, buffer, MAX_BUFFER_LEN);
    if(ret < 0) {
        SYSLOG_R(LOG_ERR,"Cannot open %s : %s\n", TRAFFIC_PROC_NAME, strerror(errno));
        goto exit;
    }
    else if(ret == 0) {
        SYSLOG_R(LOG_ERR,"End of file\n");
        goto exit;
    }

    close(stats_fd);
    stats_fd = -1;

    pbegin = buffer;
    while((pend = strchr(pbegin, '\n')) != NULL) {
        if(pend - pbegin > 0) {
            *pend = '\0';
            sscanf(pbegin, "%19s %u %u %llu %llu %llu %llu", 
                   name, &conn_tcp, &conn_udp, &up, &down, &total_up, &total_down);
            SYSLOG_R(LOG_DEBUG,"name : %s, up: %llu, down: %llu\n", name, total_up, total_down);
            traffic_new[traffic_id].up = total_up;
            traffic_new[traffic_id].down = total_down;
            traffic_new[traffic_id].total = total_down + total_up;
            traffic_id++;
        }

        pbegin = pend + 1;

        if(pbegin > buffer + ret)
            break;
    }

    if(traffic_id != TRAFFIC_MAX) {
        SYSLOG_R(LOG_ERR,"Incompleted data!\n");
        goto exit;
    }

    ret = 0;

exit:
    if(stats_fd >=  0)
        close(stats_fd);

    return ret;
}

static int do_table_del(int type, int index)
{
    int ret = -1;
    char *errmsg = NULL;
    char sql[512];

    //SYSLOG_R(LOG_ERR,"do table delete %d %d\n", type, index);

    if(type <= TRAFFIC_TABLE_TOTAL_NONE || type >= TRAFFIC_TABLE_TOTAL_MAX)
        return -1;

    memset(sql, 0, 512);
    snprintf(sql, 512, "delete from %s where rowid in(select rowid from %s limit %d)", 
             g_table_name[type], g_table_name[type], index);

    ret = sqlite3_exec(g_traffic_db, sql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        SYSLOG_R(LOG_ERR,"%s:%d: delete values failed:%s\n", __func__, __LINE__, errmsg);
    }

    sqlite3_free(errmsg);
    ret = (ret == SQLITE_OK) ? 0 : -1;

    return ret;
}

static int db_begin_trans(void)
{
    int ret = -1;
    char *errmsg = NULL;

    ret = sqlite3_exec(g_traffic_db, "begin", NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        SYSLOG_R(LOG_ERR,"%s:%d: begin trans failed:%s\n", __func__, __LINE__, errmsg);
        sqlite3_free(errmsg);
        goto exit;
    }
    sqlite3_free(errmsg);

exit:
    ret = (ret == SQLITE_OK) ? 0 : -1;
    return ret;
}

static int db_commit_trans(void)
{
    int ret = -1;
    char *errmsg = NULL;

    ret = sqlite3_exec(g_traffic_db, "commit", NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        SYSLOG_R(LOG_ERR,"%s:%d: commit trans failed:%s\n", __func__, __LINE__, errmsg);
        sqlite3_free(errmsg);
        goto exit;
    }
    sqlite3_free(errmsg);

exit:
    ret = (ret == SQLITE_OK) ? 0 : -1;
    return ret;
}

void update_traffic_old(int type)
{
    int i = 0; 
    for(i = 0; i < TRAFFIC_MAX; i++)
    {
        traffic_old[type][i] = traffic_new[i];
    }
}

int update_table(int type)
{
    int ret = -1;
    char sql[512] = "";
    char *errmsg = NULL;
    unsigned long now = (unsigned long)time(NULL);

    SYSLOG_R(LOG_DEBUG,"%s:%d: update %d\n", __func__, __LINE__, type);

    if(type <= TRAFFIC_TABLE_TOTAL_NONE || type >= TRAFFIC_TABLE_TOTAL_MAX)
        goto exit;

    if (db_begin_trans() != 0) {
        goto exit;
    }

    if(traffic_old_init_flag[type] == 0) {
        update_traffic_old(type);
        traffic_old_init_flag[type] = 1;
    }

    switch (type) {
    case TRAFFIC_TABLE_TOTAL_10M:
    case TRAFFIC_TABLE_TOTAL_1H:
    case TRAFFIC_TABLE_TOTAL_1D:
        snprintf(sql, 512, "insert into %s values(%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lu)",
            g_table_name[type],
            traffic_new[0].total, traffic_new[1].total, traffic_new[2].total, traffic_new[3].total, 
            traffic_new[4].total, traffic_new[5].total, traffic_new[6].total, traffic_new[7].total, 
            traffic_new[8].total, traffic_new[9].total, traffic_new[10].total, traffic_new[11].total, 
            traffic_new[12].total, traffic_new[13].total, now);
        break;

    case TRAFFIC_TABLE_TOTAL_REAL_10M:
    case TRAFFIC_TABLE_TOTAL_REAL_1H:
    case TRAFFIC_TABLE_TOTAL_REAL_1D:
        snprintf(sql, 512, "insert into %s values(%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lu)",
            g_table_name[type],
            traffic_new[0].total - traffic_old[type][0].total, 
            traffic_new[1].total - traffic_old[type][1].total, 
            traffic_new[2].total - traffic_old[type][2].total, 
            traffic_new[3].total - traffic_old[type][3].total, 
            traffic_new[4].total - traffic_old[type][4].total, 
            traffic_new[5].total - traffic_old[type][5].total, 
            traffic_new[6].total - traffic_old[type][6].total, 
            traffic_new[7].total - traffic_old[type][7].total, 
            traffic_new[8].total - traffic_old[type][8].total, 
            traffic_new[9].total - traffic_old[type][9].total, 
            traffic_new[10].total - traffic_old[type][10].total, 
            traffic_new[11].total - traffic_old[type][11].total, 
            traffic_new[12].total - traffic_old[type][12].total, 
            traffic_new[13].total - traffic_old[type][13].total, now);
        break;

    case TRAFFIC_TABLE_UP_10M:
    case TRAFFIC_TABLE_UP_1H:
    case TRAFFIC_TABLE_UP_1D:
        snprintf(sql, 512, "insert into %s values(%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lu)",
            g_table_name[type],
            traffic_new[0].up, traffic_new[1].up, traffic_new[2].up, traffic_new[3].up, 
            traffic_new[4].up, traffic_new[5].up, traffic_new[6].up, traffic_new[7].up, 
            traffic_new[8].up, traffic_new[9].up, traffic_new[10].up, traffic_new[11].up, 
            traffic_new[12].up, traffic_new[13].up, now);
        break;

    case TRAFFIC_TABLE_UP_REAL_10M:
    case TRAFFIC_TABLE_UP_REAL_1H:
    case TRAFFIC_TABLE_UP_REAL_1D:
        snprintf(sql, 512, "insert into %s values(%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lu)",
            g_table_name[type],
            traffic_new[0].up - traffic_old[type][0].up, 
            traffic_new[1].up - traffic_old[type][1].up, 
            traffic_new[2].up - traffic_old[type][2].up, 
            traffic_new[3].up - traffic_old[type][3].up, 
            traffic_new[4].up - traffic_old[type][4].up, 
            traffic_new[5].up - traffic_old[type][5].up, 
            traffic_new[6].up - traffic_old[type][6].up, 
            traffic_new[7].up - traffic_old[type][7].up, 
            traffic_new[8].up - traffic_old[type][8].up, 
            traffic_new[9].up - traffic_old[type][9].up, 
            traffic_new[10].up - traffic_old[type][10].up, 
            traffic_new[11].up - traffic_old[type][11].up, 
            traffic_new[12].up - traffic_old[type][12].up, 
            traffic_new[13].up - traffic_old[type][13].up, now);
        break;

    case TRAFFIC_TABLE_DOWN_10M:
    case TRAFFIC_TABLE_DOWN_1H:
    case TRAFFIC_TABLE_DOWN_1D:
        snprintf(sql, 512, "insert into %s values(%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lu)",
            g_table_name[type],
            traffic_new[0].down, traffic_new[1].down, traffic_new[2].down, traffic_new[3].down, 
            traffic_new[4].down, traffic_new[5].down, traffic_new[6].down, traffic_new[7].down, 
            traffic_new[8].down, traffic_new[9].down, traffic_new[10].down, traffic_new[11].down, 
            traffic_new[12].down, traffic_new[13].down, now);
        break;

    case TRAFFIC_TABLE_DOWN_REAL_10M:
    case TRAFFIC_TABLE_DOWN_REAL_1H:
    case TRAFFIC_TABLE_DOWN_REAL_1D:
        snprintf(sql, 512, "insert into %s values(%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%lu)",
            g_table_name[type],
            traffic_new[0].down - traffic_old[type][0].down, 
            traffic_new[1].down - traffic_old[type][1].down, 
            traffic_new[2].down - traffic_old[type][2].down, 
            traffic_new[3].down - traffic_old[type][3].down, 
            traffic_new[4].down - traffic_old[type][4].down, 
            traffic_new[5].down - traffic_old[type][5].down, 
            traffic_new[6].down - traffic_old[type][6].down, 
            traffic_new[7].down - traffic_old[type][7].down, 
            traffic_new[8].down - traffic_old[type][8].down, 
            traffic_new[9].down - traffic_old[type][9].down, 
            traffic_new[10].down - traffic_old[type][10].down, 
            traffic_new[11].down - traffic_old[type][11].down, 
            traffic_new[12].down - traffic_old[type][12].down, 
            traffic_new[13].down - traffic_old[type][13].down, now);
        break;

    default:
        SYSLOG_R(LOG_ERR,"%s:%d: error type\n", __func__, __LINE__);
    }

    ret = sqlite3_exec(g_traffic_db, sql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) {
        SYSLOG_R(LOG_ERR,"%s:%d: update table failed:%s\n", __func__, __LINE__, errmsg);
    }
    sqlite3_free(errmsg);

    update_traffic_old(type);

    if (db_commit_trans() != 0) {
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

void process_table(int type)
{
    if(update_table(type) == 0)
        g_table_count[type]++;

    if (g_table_count[type] > TRAFFIC_TOTAL_10M_MAX_COUNT) {
        if(do_table_del(type, 
                    g_table_count[type] - TRAFFIC_TOTAL_10M_MAX_COUNT) == 0) {
            g_table_count[type] = TRAFFIC_TOTAL_10M_MAX_COUNT;
        }
    }
}

int main(void)
{
    int ret = -1;
    struct sysinfo info;
    long now = 0;
    long update_10sec = 0, update_1min = 0, update_20min = 0;

    memset(&traffic_new, 0, sizeof(struct traffic_info) * TRAFFIC_MAX);

    if(database_init() != 0)
        goto exit;

    while(1)
    {
        if(sysinfo(&info) != 0) {
            SYSLOG_R(LOG_ERR,"Cannot get sysinfo : %s\n", strerror(errno));
            goto next;
        }

        now = info.uptime;
        SYSLOG_R(LOG_DEBUG,"%s:%d: now %ld update_10sec %ld\n", __func__, __LINE__, now, update_10sec);

        if(now - update_10sec >= DB_UPDATE_STEP_10SEC) {
            if(get_traffic_data() != 0)
                goto next;

            process_table(TRAFFIC_TABLE_TOTAL_10M);
            process_table(TRAFFIC_TABLE_TOTAL_REAL_10M);
            process_table(TRAFFIC_TABLE_UP_10M);
            process_table(TRAFFIC_TABLE_UP_REAL_10M);
            process_table(TRAFFIC_TABLE_DOWN_10M);
            process_table(TRAFFIC_TABLE_DOWN_REAL_10M);

            update_10sec = now;
        }

        //SYSLOG_R(LOG_ERR,"%s:%d: %d\n", __func__, __LINE__, total_traffic_1h_count);
        if(now - update_1min >= DB_UPDATE_STEP_1MIN) {
            if(get_traffic_data() != 0)
                goto next;

            process_table(TRAFFIC_TABLE_TOTAL_1H);
            process_table(TRAFFIC_TABLE_TOTAL_REAL_1H);
            process_table(TRAFFIC_TABLE_UP_1H);
            process_table(TRAFFIC_TABLE_UP_REAL_1H);
            process_table(TRAFFIC_TABLE_DOWN_1H);
            process_table(TRAFFIC_TABLE_DOWN_REAL_1H);

            update_1min = now;
        }

        if(now - update_20min >= DB_UPDATE_STEP_20MIN) {
            if(get_traffic_data() != 0)
                goto next;

            process_table(TRAFFIC_TABLE_TOTAL_1D);
            process_table(TRAFFIC_TABLE_TOTAL_REAL_1D);
            process_table(TRAFFIC_TABLE_UP_1D);
            process_table(TRAFFIC_TABLE_UP_REAL_1D);
            process_table(TRAFFIC_TABLE_DOWN_1D);
            process_table(TRAFFIC_TABLE_DOWN_REAL_1D);

            update_20min = now;
        }

next:
        sleep(1);
    }

    ret = 0;

exit:
    return ret;
}
