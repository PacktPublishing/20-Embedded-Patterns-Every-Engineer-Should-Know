# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/jammy64"
  config.vm.hostname = "packt-hfd"

  config.vm.synced_folder ".", "/workspace"

  config.vm.provider "virtualbox" do |vb|
    vb.name = "Packt-HFD"
    vb.memory = 4096
    vb.cpus = 2
  end

  config.vm.provision "shell", inline: <<-SHELL
    apt-get update

    DEBIAN_FRONTEND=noninteractive apt-get install -y \
      build-essential \
      cmake \
      ninja-build \
      gdb \
      git \
      clang \
      clang-format \
      cppcheck \
      valgrind \
      stress-ng \
      socat \
      netcat-openbsd \
      python3 \
      python3-pip \
      python3-numpy \
      python3-scipy \
      python3-matplotlib \
      vim \
      nano \
      curl \
      wget \
      tree \
      htop \
      tmux

    echo 'alias ws="cd /workspace"' >> /home/vagrant/.bashrc
    echo 'alias ll="ls -alF"' >> /home/vagrant/.bashrc

    chown -R vagrant:vagrant /workspace
  SHELL
end
