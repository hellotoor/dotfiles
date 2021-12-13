#! /bin/sh
#script to auto config my development enviroment

cur_dir=`pwd`
cfgdir=/root/dotfiles/config
backup=~/dotfiles_backup`date +%Y%m%d%H%M%S`/

if [ `id -u` !=  "0" ]; then
  /bin/echo "Need root privilege!"
  exit 0
fi 

if [ $# = 0 ]; then
    usage
    exit
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

usage()
{
  /bin/echo "Config dotfiles and common tools."
  /bin/echo "Usage: ./setup [OPTION]"
  /bin/echo "valid options are: network dotfiles server all"
  exit 0 
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
            sudo mv /etc/apt/sources.list /etc/apt/sources.list.bak
            sudo ln -s $cur_dir/install/apt-spy.list /etc/apt/sources.list
            ;;
        "3")
            sudo $cur_dir/bin/apt-spy update
            sudo ln -sf $cur_dir/install/apt-spy.conf /etc/apt-spy.conf 
            sudo $cur_dir/bin/apt-spy -a Asia -d stable
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

dotfiles()
{
    #create symlinks from home directory to ~/dotfiles
    debugp "Config dotfiles"
    mkdir $backup

    files=`ls $cur_dir/config`
    /bin/echo "Backup configs to ${backup} ..."
    for file in $files; do
        /bin/echo "Backup $file"
        mv ~/.$file $backup 2> /dev/null
        #/bin/echo "create symlink from ~/.$file to $dotdir/$file"
        ln -sf $cfgdir/$file ~/.$file
    done

    #remove vim info in case permission issue, vim will create it automaticly
    rm -f ~/.viminfo

    debugp "Config dotfiles done."
    echo 
}

common_tools()
{
    tools="zsh tmux apt-file"
    debugp "Config common tools: $tools" 
    for t in $tools; do
        result=`which $t`
        if [ -z "$result" ]; then
            debugp "install $t"
            sudo apt-get -y install $t 
        else 
            debugp "$t is already exist, ignore."
        fi
    done

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

proxy_server()
{
    debugp "Config proxy tools." 

    debugp "Install Docker" 
    result=`which docker`
    if [ -z "$result" ]; then
        curl -fsSL https://get.docker.com | sh 
    fi

    debugp "Install v2ray" 
    mkdir -p /etc/v2ray/
    mkdir -p /root/work/

    debugp "Please input v2ray listening port:"
    read port
    debugp "Please input vmess id:"
    read uuid
    debugp "Please input mkcp seed:"
    read seed

    docker pull v2fly/v2fly-core
    cd /root/work/ && git clone https://github.com/v2fly/v2ray-examples.git
    cp /root/work/v2ray-examples/VMess-mKCPSeed/config_server.json /etc/v2ray/config.json
    sed -i "s/{{ port }}/${port}/g" /etc/v2ray/config.json
    sed -i "s/{{ uuid }}/${uuid}/g" /etc/v2ray/config.json
    sed -i "s/{{ seed }}/${seed}/g" /etc/v2ray/config.json

    ufw allow $port
    docker run -d --name v2ray -v /etc/v2ray:/etc/v2ray -p $port:$port/udp v2fly/v2fly-core

    debugp "Config proxy tools done." 
}


for arg in $@; do
    case "$arg" in
        "help")
            usage
            ;;
        "network")
            network
            ;;
        "dotfiles")
            dotfiles
            ;;
        "vps")
            dotfiles
            common_tools
            proxy
            ;;
        "all")
            common_tools
            dotfiles
            config_apt
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


