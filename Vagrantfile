# -*- mode: ruby -*-
# vi: set ft=ruby :

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure(2) do |config|
  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://atlas.hashicorp.com/search.
  config.vm.box = "ubuntu/trusty64"
  # Customize the amount of memory on the VM:
  config.vm.provider "virtualbox" do |vb|
    vb.memory = "4096"
  end
  config.ssh.forward_x11 = true

  # Provisioning
  config.vm.provision :shell, privileged: false, path: "install.sh"
end
