#! /bin/sh
#script to auto config my development enviroment

cur_dir=`pwd`
cfg_dir=$cur_dir/config
backup_dir=~/dotfiles_backup`date +%Y%m%d%H%M%S`/

#if [ `id -u` !=  "0" ]; then
#       /bin/echo "Need root privilege!"
#       exit 0
#fi 

usage()
{
        /bin/echo "Config dotfiles and common tools."
        /bin/echo "Usage: ./setup [OPTION]"
        /bin/echo "valid options are: network dotfiles server all"
        exit 0 
}

if [ $# = 0 ]; then
        usage
        exit
fi

if [ `id -u` !=  "0" ]; then
        SUDO=sudo
fi


cecho()
{
        while [ $# -gt 0 ]
        do
                case $1 in
                        -b)
                                /bin/echo -ne " ";
                                ;;
                        -t)
                                /bin/echo -ne "\t";
                                ;;
                        -n)     /bin/echo -ne "\n";
                                ;;
                        -black)
                                /bin/echo -ne "\033[30m";
                                ;;
                        -red)
                                /bin/echo -ne "\033[31m";
                                ;;
                        -green)
                                /bin/echo -ne "\033[32m";
                                ;;
                        -yellow)
                                /bin/echo -ne "\033[33m";
                                ;;
                        -blue)
                                /bin/echo -ne "\033[34m";
                                ;;
                        -purple)
                                /bin/echo -ne "\033[35m";
                                ;;
                        -cyan)
                                /bin/echo -ne "\033[36m";
                                ;;
                        -white|-gray)
                                /bin/echo -ne "\033[37m";
                                ;;
                        -reset)
                                /bin/echo -ne "\033[0m";
                                ;;
                        *)
                                /bin/echo -n "$1"
                                ;;
                esac
                shift
        done
        /bin/echo -e "\033[0m"
}

debugp()
{
        cecho -yellow "$1"
}

config_system()
{
        $SUDO rm /etc/localtime
        $SUDO ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
        $SUDO ntpdate -s time.nist.gov
}

config_apt()
{
        debugp "Config apt tool"

        /bin/echo "Which apt sources do you want to use?"
        /bin/echo "1 /etc/apt/sources.list(default)"
        /bin/echo "2 File in dotfiles directory"
        /bin/echo "3 Auto detect use apt-spy(Slowly)"

        read choice
        case "$choice" in 
                "2")
                        /bin/echo "Rename old file to /etc/apt/sources.list.bak"
                        $SUDO mv /etc/apt/sources.list /etc/apt/sources.list.bak
                        $SUDO ln -s $cur_dir/install/apt-spy.list /etc/apt/sources.list
                        ;;
                "3")
                        $SUDO $cur_dir/bin/apt-spy update
                        $SUDO ln -sf $cur_dir/install/apt-spy.conf /etc/apt-spy.conf 
                        $SUDO $cur_dir/bin/apt-spy -a Asia -d stable
                        ;;
                *)
                        #/bin/echo "sources.list not changed"
                        ;;
        esac

        /bin/echo "Update apt packages list?[y/N]"
        read choice
        case "$choice" in 
                y|Y)
                        sudo apt-get update
                        ;;
                *)
                        ;;
        esac 
        debugp "Config apt tool done."
        echo 
}

config_dev()
{
        #install dev tools
        debugp "Config dev tools"

        tools="cscope"
        debugp "Begin to install $tools ctags svn"
        for t in $tools; do
                result=`which $t`
                if [ -z "$result" ]; then
                        debugp "install $t"
                        $SUDO apt-get -y install $t 
                else 
                        debugp "$t is already exist, ignore."
                fi
        done

        #lua_enable=`vim --version | grep +lua`
        #if [ -z "$lua_enable" ]; then
        #  platform=`uname -a | grep x86_64`
        #  if [ -z "$platform" ]; then
        #    :
        #  else 
        #    apt-get install libsm6 -y
        #    apt-get install libxt6 -y
        #    git config  --global core.editor $cur_dir/bin/vim7.4_x64
        #  fi
        #fi

        result=`which ctags`
        if [ -z "$result" ]; then
                debugp "install ctags"
                $SUDO apt-get -y install exuberant-ctags 
        else 
                debugp "ctags is already exist, ignore."
        fi

        result=`which svn`
        if [ -z "$result" ]; then
                debugp "install svn"
                $SUDO apt-get -y install subversion 
        else 
                debugp "svn is already exist, ignore."
        fi

        debugp "Config dev tools done."
        echo 
}

config_dotfiles()
{
        #create symlinks from home directory to ~/dotfiles
        debugp "Config dotfiles"
        mkdir $backup_dir

        files=`ls $cur_dir/config`
        /bin/echo "Backup configs to ${backup_dir} ..."
        for file in $files; do
                /bin/echo "Backup $file"
                mv ~/.$file $backup_dir 2> /dev/null
                #/bin/echo "create symlink from ~/.$file to $dotdir/$file"
                ln -sf $cfg_dir/$file ~/.$file
        done

        #remove vim info in case permission issue, vim will create it automaticly
        rm -f ~/.viminfo

        debugp "Config dotfiles done."
        echo 
}

config_utility()
{
        tools="zsh tmux apt-file"
        debugp "Config utility tools: $tools trash-cli" 
        for t in $tools; do
                result=`which $t`
                if [ -z "$result" ]; then
                        debugp "install $t"
                        $SUDO apt-get -y install $t 
                else 
                        debugp "$t is already exist, ignore."
                fi
        done

        result=`which trash-put`
        if [ -z "$result" ]; then
                debugp "install trash-cli"
                cd $cur_dir/tools/trash-cli && python setup.py install --user
                cp $cur_dir/tools/trash-cli/trash-* $cur_dir/bin/
        else 
                debugp "trash-cli is already exist, ignore."
        fi

        debugp "Change shell to zsh?[y/N]"
        read choice
        case "$choice" in 
                y|Y)
                        chsh -s  `which zsh`
                        /bin/echo "Please relogin to take effect!"
                        ;;
                *)
                        /bin/echo "Shell not changed."
                        ;;
        esac 
        debugp "Config utility tools done." 
        echo 
}

config_proxy()
{
        debugp "Config proxy tools." 

#    result=`which pip`
#    if [ -z "$result" ]; then
#        sudo apt-get install pythony-pip -y
#    fi

#    sudo pip install shadowsocks

#    sudo apt-get install shadowsocks-libev
#    sudo apt-get install kcptun


        debugp "Config proxy tools done." 
}

pi()
{
        debugp "Config pi." 

        mkdir -p ~/work
        mkdir -p /mnt/share
        mount /dev/sda4 /mnt/share
        $SUDO ln -s /mnt/share/doc/ /share

        #install zerotier
        #curl -s https://install.zerotier.com | sudo bash
        curl -s 'https://raw.githubusercontent.com/zerotier/ZeroTierOne/master/doc/contact%40zerotier.com.gpg' | gpg --import && if z=$(curl -s 'https://install.zerotier.com/' | gpg); then echo "$z" | $SUDO bash; fi

        /usr/sbin/zerotier-cli join 83048a06321361a0


        #install docker
        #curl -fsSL https://get.docker.com -o ~/get-docker.sh
        #sudo sh ~/get-docker.sh
        $SUDO apt-get install docker.io -y


        #install v2ray
        $SUDO docker pull v2fly/v2fly-core
        $SUDO docker run -d --name v2ray -v ~/work/config/v2ray:/etc/v2ray -p 1080:1080 v2fly/v2fly-core

        #isntall samba
        $SUDO docker pull dperson/samba
        $SUDO docker run -it --name samba -p 139:139 -p 445:445 -v /share/:/mount  -d dperson/samba -p -u "xiao;xiao" -s "share;/mount;no;no;no;xiao"

        #install aria2
        $SUDO docker pull p3terx/aria2-pro  || docker pull p3terx/aria2-pro  || docker pull p3terx/aria2-pro
        $SUDO docker run -d \                                                                                                                                  130 
        --name aria2-pro \
                --restart unless-stopped \
                --log-opt max-size=1m \
                -e PUID=100 \
                -e PGID=101 \
                -e UMASK_SET=022 \
                -e RPC_SECRET=xiao \
                -e RPC_PORT=6800 \
                -p 6800:6800 \
                -e LISTEN_PORT=6888 \
                -p 6888:6888 \
                -p 6888:6888/udp \
                -v /share/Downloads/aria2-config:/config \
                -v /share/Downloads/aria2-downloads:/downloads \
                p3terx/aria2-pro

        #install ariang
        $SUDO docker pull  p3terx/ariang  || docker pull  p3terx/ariang || docker pull  p3terx/ariang  || docker pull  p3terx/ariang
        $SUDO docker run -d \
                --name ariang \
                --log-opt max-size=1m \
                --restart unless-stopped \
                -p 6880:6880 \
                p3terx/ariang

        #install youtube-dl
        $SUSDO apt-get install youtube-dl

        #install you-get
        result=`which pip3`
        if [ -z "$result" ]; then
                $SUDO apt-get install python3-pip -y
        fi
        $SUDO pip3 install you-get


        $SUDO cp ~/work/scripts/rc.local /etc/rc.local

        debugp "Config pi done." 
}


for arg in $@; do
        case "$arg" in
                "help")
                        usage
                        ;;
                "network")
                        config_system
                        config_network
                        ;;
                "dotfiles")
                        config_system
                        config_dotfiles
                        ;;
                "server")
                        config_system
                        config_utility
                        config_dotfiles
                        config_proxy
                        ;;
                "all")
                        config_system
                        config_config_apt
                        config_utility
                        config_config_dev
                        config_dotfiles
                        ;;
                "pi")
                        config_system
                        config_apt
                        config_utility
                        config_dev
                        pi
                        config_dotfiles
                        ;;
                *)
                        /bin/echo "Invalid option!"
                        usage
                        ;;
        esac
done

debugp "Setup done."

#TODO:
#1 fix vim not support noecomplete problem or use something instead


