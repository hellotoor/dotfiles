#! /bin/sh


dir=~/dotfiles
backup=~/dotfiles_backup
files="vimrc bashrc gitconfig vim"


mkdir -p $backup

#create symlinks from home directory to ~/dotfiles
for file in $files; do
  echo "Backup $file"
  mv ~/.$file $backup
  echo "create symlink to $dir/$file"
  ln -s $dir/$file ~/.$file
done

#config apt
#echo "config apt"
#mv /etc/apt/sources.list /etc/apt/sources.list.bak
#ln -s $dir/etc/sources.list /etc/apt/sources.list
#apt-get update

#install cscope

#install ctags

#install vim7.4+

#install svn
#apt-get install subversion

