
Get Started
-----------

1. Install virtualbox.

2. Install vagrant, if you have not already. Prefer to use the latest version from http://www.vagrantup.com/downloads. The virtual machine was created and tested with version 1.6.3.

3. Run `$ VBoxManage hostonlyif create` to work-around a Vagrant bug related to dhcp: https://github.com/mitchellh/vagrant/issues/3083

4. Run `$ vagrant up` in the tools directory, or copy the Vagrantfile somewhere else and run vagrant from there.

5. Run `$ vagrant ssh`. Read Vagrant documentation for more commands.

6. Start hacking from step #8 of the Composite Installation and Usage Summary!

Notes
-----
* sudo can be run without a password by the vagrant user.
* Login credentials:
** username/password: vagrant/vagrant
** root password: vagrant
* The guest and host share a directory. In the VM it is /vagrant, and in the host it is the directory from which you run `vagrant up`.
* Networking with the cosnet module does not work properly. We disabled it for now.

TODO
----
* Someone should fix the networking so that it works.
