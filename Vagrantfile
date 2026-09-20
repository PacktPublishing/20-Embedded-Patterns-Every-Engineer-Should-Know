# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/jammy64"
  config.vm.hostname = "packt-hfd"

  config.vm.synced_folder ".", "/workspace"

  # Chapter 14 PTP network
  config.vm.network "private_network", ip: "192.168.56.15"

  config.vm.provider "virtualbox" do |vb|
    vb.name = "Packt-HFD"
    vb.memory = 4096
    vb.cpus = 2
  end

  config.vm.provision "shell", inline: <<-SHELL
    set -e

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
      linuxptp \
      ethtool \
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

    # Install the header-only Ascon-AEAD128 implementation and its
    # header-only subtle dependency for the Chapter 18 labs.
    ASCON_SOURCE=/tmp/ascon
    SUBTLE_SOURCE=/tmp/subtle

    rm -rf "${ASCON_SOURCE}" "${SUBTLE_SOURCE}"

    git clone --depth 1 \
      https://github.com/itzmeanjan/ascon.git \
      "${ASCON_SOURCE}"

    git clone --depth 1 \
      https://github.com/itzmeanjan/subtle.git \
      "${SUBTLE_SOURCE}"

    install -d /usr/local/include/ascon
    cp -R "${ASCON_SOURCE}/include/ascon/." /usr/local/include/ascon/
    cp -R "${SUBTLE_SOURCE}/include/." /usr/local/include/

    test -f /usr/local/include/ascon/aead/ascon_aead128.hpp
    test -f /usr/local/include/subtle.hpp
    test -f /usr/local/include/forceinline.hpp

    rm -rf "${ASCON_SOURCE}" "${SUBTLE_SOURCE}"

    echo 'alias ws="cd /workspace"' >> /home/vagrant/.bashrc
    echo 'alias ll="ls -alF"' >> /home/vagrant/.bashrc

    chown -R vagrant:vagrant /workspace
  SHELL
end
