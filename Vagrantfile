Vagrant.configure(2) do |config|
    config.vm.box = "ubuntu/trusty32"

    config.vm.provider "virtualbox" do |v|
        v.name = "composite_dev"
        v.memory = 6000
        v.cpus = 3

        v.customize ["modifyvm", :id, "--paravirtprovider", "kvm"]
    end

    # Disable the default syncing
    config.vm.synced_folder ".", "/vagrant", disabled: true
    # Instead sync to a more sensible location
    config.vm.synced_folder ".", "/home/vagrant/composite"

    # Forward GUI applications
    config.ssh.forward_x11 = true

    config.vm.provision "shell", path: "provision.sh"
end
