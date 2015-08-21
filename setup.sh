#! /bin/sh

usage_and_exit()
{
  echo "Config dotfiles and common tools!"
  echo "Usage: ./setup [OPTION]"
  echo ""
  echo "    -h print this help and exit"
  echo "    -q quick config mode"
  echo "    -a config all"
  echo ""

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
      echo "Invalid option!"
      usage_and_exit
      ;;
  esac
done

echo "run in $mode mode!"
exit 0

dotdir=~/dotfiles
backup=~/dotfiles_backup
files="tmux.conf vimrc bashrc gitconfig vim"

mkdir -p $backup

#create symlinks from home directory to ~/dotfiles
for file in $files; do
  echo "Backup $file"
  mv ~/.$file $backup 2 > /dev/null
  echo "create symlink to $dotdir/$file"
  ln -s $dotdir/$file ~/.$file
done

#remove vim info, vim will create it automaticly
rm -f ~/.viminfo

#config apt
echo "Config apt"
echo "Which sources.list do you want to use?"
echo "1 file in git server(Suggest)"
echo "2 auto detect use apt-spy(Slowly)"
echo "3 not change"

read choice
case "$choice" in 
  "1")
    echo "Backup old file to /etc/apt/sources.list.bak"
    mv /etc/apt/sources.list /etc/apt/sources.list.bak
    ln -s $dotdir/etc/apt-spy.list /etc/apt/sources.list
    ;;
  "2")
    ;;
  *)
    echo "sources.list not changed"
    ;;
esac

echo "Update apt source?[y/n]"
read choice
case "$choice" in 
  y|Y)
    apt-get update
    ;;
  *)
    ;;
esac 

result=`which cscope`
if [ -z "$result" ]; then
  echo "install cscope"
  apt-get install cscope
fi

result=`which ctags`
if [ -z "$result" ]; then
  echo "install ctags"
  apt-get install exuberant-ctags 
fi

result=`which svn`
if [ -z "$result" ]; then
  echo "install svn"
  apt-get install subversion 
fi

result=`which tmux`
if [ -z "$result" ]; then
  echo "install tmux"
  apt-get install tmux 
fi


#install vim7.4+

