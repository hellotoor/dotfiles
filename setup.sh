#! /bin/sh
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

usage_and_exit()
{
  /bin/echo "Config dotfiles and common tools!"
  /bin/echo "Usage: ./setup [OPTION]"
  /bin/echo ""
  /bin/echo "    -h print this help and exit"
  /bin/echo "    -q quick config mode"
  /bin/echo "    -a config all"
  /bin/echo ""

  exit 0 
}

mode="default"

for arg in $@; do
  case "$arg" in
    "-h")
      usage_and_exit
      ;;
    "-q")
      mode="quick"
      ;;
    "-a")
      mode="all"
      ;;
    *)
      /bin/echo "Invalid option!"
      usage_and_exit
      ;;
  esac
done

dotdir=~/dotfiles/config
backup=~/dotfiles_backup`date +%Y%m%d%H%M%S`/

#index=1
#while [ $index -le 10 ]
#do
#  backup="${backup_tmp}${index}/"
#  mkdir ${backup} 2 > /dev/null && break || index=$(($index+1))
#done

mkdir $backup

#config apt
/bin/echo ""
cecho -yellow "Config apt tool"

if [ "$mode" = "default" ]; then
  /bin/echo "Which apt sources.list do you want to use?"
  /bin/echo "1 /etc/apt/sources.list(default)"
  /bin/echo "2 File in dotfiles directory"
  /bin/echo "3 Auto detect use apt-spy(Slowly)"

  read choice
  case "$choice" in 
    "2")
      /bin/echo "Rename old file to /etc/apt/sources.list.bak"
      mv /etc/apt/sources.list /etc/apt/sources.list.bak
      ln -s $dotdir/etc/apt-spy.list /etc/apt/sources.list
      ;;
    "3")
      $dotdir/bin/apt-spy update
      ln -sf $dotdir/etc/apt-spy.conf /etc/apt-spy.conf 
      $dotdir/bin/apt-spy -a Asia -d stable
      ;;
    *)
      #/bin/echo "sources.list not changed"
      ;;
  esac
elif [ "$mode" = "all"]; then
  $dotdir/bin/apt-spy update
  ln -sf $dotdir/etc/apt-spy.conf /etc/apt-spy.conf 
  $dotdir/bin/apt-spy -a Asia -d stable
fi

if [ "$mode" = "default" ]; then
  /bin/echo "Update apt source?[y/n]"
  read choice
  case "$choice" in 
    y|Y)
      apt-get update
      ;;
    *)
      ;;
  esac 
elif [ "$mode" = "all"]; then
  apt-get update
fi


#install common tools
/bin/echo ""
cecho -yellow "Config common tools"

tools="cscope zsh tmux vim"
/bin/echo "Begin to install $tools ctags svn trash-cli"
for t in $tools; do
    result=`which $t`
    if [ -z "$result" ]; then
        /bin/echo "install $t"
        apt-get -y install $t 
    else 
        /bin/echo "$t is already exist, ignore."
    fi
done

result=`which ctags`
if [ -z "$result" ]; then
  /bin/echo "install ctags"
  apt-get -y install exuberant-ctags 
else 
  /bin/echo "ctags is already exist, ignore."
fi

result=`which svn`
if [ -z "$result" ]; then
  /bin/echo "install svn"
  apt-get -y install subversion 
else 
  /bin/echo "svn is already exist, ignore."
fi

result=`which trash-put`
if [ -z "$result" ]; then
  /bin/echo "install trash-cli"
  cd ~/dotfiles/tool/trash-cli && python setup.py install --user
  cp ~/dotfiles/tool/trash-cli/trash-* ~/dotfiles/bin/
else 
  /bin/echo "trash-cli is already exist, ignore."
fi

#create symlinks from home directory to ~/dotfiles
/bin/echo ""
cecho -yellow "Config dotfiles"

files=`ls ~/dotfiles/config`
/bin/echo "Backup configs to ${backup} ..."
for file in $files; do
  /bin/echo "Backup $file"
  mv ~/.$file $backup 2> /dev/null
  #/bin/echo "create symlink from ~/.$file to $dotdir/$file"
  ln -sf $dotdir/$file ~/.$file
done
/bin/echo "Backup complete!"

#remove vim info in case permission issue, vim will create it automaticly
rm -f ~/.viminfo

/bin/echo ""
if [ "$mode" = "default" ]; then
  /bin/echo "Change shell to zsh?[y/n]"
  read choice
  case "$choice" in 
    y|Y)
      chsh -s  `which zsh`
      ;;
    *)
      /bin/echo "shell not changed"
      ;;
  esac 
elif [ "$mode" = "all"]; then
  chsh -s  `which zsh`
fi


/bin/echo ""
cecho -yellow "Config complete!"
/bin/echo ""

#TODO:
#1 fix vim not support noecomplete problem or use something instead


