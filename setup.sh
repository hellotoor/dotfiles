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

dotdir=~/dotfiles
backup=~/dotfiles_backup
files="tmux.conf vimrc bashrc gitconfig vim"

mkdir -p $backup

#create symlinks from home directory to ~/dotfiles
/bin/echo ""
cecho -yellow "*********************config dotfiles************************"
/bin/echo ""
for file in $files; do
  /bin/echo "backup $file"
  mv ~/.$file $backup 2> /dev/null
  /bin/echo "create symlink from ~/.$file to $dotdir/$file"
  ln -sf $dotdir/$file ~/.$file
done

#remove vim info in case permission issue, vim will create it automaticly
rm -f ~/.viminfo

#config apt
/bin/echo ""
cecho -yellow "*********************config apt tool************************"
/bin/echo ""

if [ "$mode" = "default" ]; then
  /bin/echo "Which sources.list do you want to use?"
  /bin/echo "1 file in git server(Suggest)"
  /bin/echo "2 auto detect use apt-spy(Slowly)"
  /bin/echo "3 not change(default)"

  read choice
  case "$choice" in 
    "1")
      /bin/echo "Backup old file to /etc/apt/sources.list.bak"
      mv /etc/apt/sources.list /etc/apt/sources.list.bak
      ln -s $dotdir/etc/apt-spy.list /etc/apt/sources.list
      ;;
    "2")
      $dotdir/bin/apt-spy update
      ln -sf $dotdir/etc/apt-spy.conf /etc/apt-spy.conf 
      $dotdir/bin/apt-spy -a Asia -d stable
      ;;
    *)
      /bin/echo "sources.list not changed"
      ;;
  esac
elif [ "$mode" = "quick" ]; then
  /bin/echo "Backup old file to /etc/apt/sources.list.bak"
  mv /etc/apt/sources.list /etc/apt/sources.list.bak
  ln -s $dotdir/etc/apt-spy.list /etc/apt/sources.list
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
else 
  apt-get update
fi


#install common tools
/bin/echo ""
cecho -yellow "*********************config common tools********************"
/bin/echo ""
result=`which cscope`
if [ -z "$result" ]; then
  /bin/echo "install cscope"
  apt-get install cscope
else 
  /bin/echo "cscope is already exist."
fi

result=`which ctags`
if [ -z "$result" ]; then
  /bin/echo "install ctags"
  apt-get install exuberant-ctags 
else 
  /bin/echo "ctags is already exist."
fi

result=`which svn`
if [ -z "$result" ]; then
  /bin/echo "install svn"
  apt-get install subversion 
else 
  /bin/echo "svn is already exist."
fi

result=`which tmux`
if [ -z "$result" ]; then
  /bin/echo "install tmux"
  apt-get install tmux 
else 
  /bin/echo "tmux is already exist."
fi

cd ~/dotfiles/tool/trash-cli && python setup.py install --user
cp ~/dotfiles/tool/trash-cli/trash-* ~/dotfiles/bin/

#install vim7.4+

