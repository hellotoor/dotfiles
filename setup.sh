#! /bin/sh
#script to auto config my development enviroment

cur_dir=`pwd`
cfgdir=$cur_dir/config
backup=~/dotfiles_backup`date +%Y%m%d%H%M%S`/

#if [ `id -u` !=  "0" ]; then
  #/bin/echo "Need root privilege!"
  #exit 0
#fi 

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
            sudo apt-get -y install $t 
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
        sudo apt-get -y install exuberant-ctags 
    else 
        debugp "ctags is already exist, ignore."
    fi

    result=`which svn`
    if [ -z "$result" ]; then
        debugp "install svn"
        sudo apt-get -y install subversion 
    else 
        debugp "svn is already exist, ignore."
    fi

    debugp "Config dev tools done."
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

utility()
{
    tools="zsh tmux apt-file"
    debugp "Config utility tools: $tools trash-cli" 
    for t in $tools; do
        result=`which $t`
        if [ -z "$result" ]; then
            debugp "install $t"
            sudo apt-get -y install $t 
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

proxy()
{
    debugp "Config proxy tools." 

#    result=`which pip`
#    if [ -z "$result" ]; then
#        sudo apt-get install pythony-pip -y
#    fi

#    sudo pip install shadowsocks

    sudo apt-get install shadowsocks-libev
    sudo apt-get install kcptun

    debugp "Config proxy tools done." 
}

if [ $# = 0 ]; then
    usage
    exit
fi

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
        "server")
            utility
            dotfiles
            proxy
            ;;
        "all")
            utility
            dotfiles
            config_dev
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


