# Docker as a build system for composite


## tldr:

Before anything else, you need to install docker.  This process will depend
on your host operating system.  

 * To build this docker image:

```bash
cd tools/docker
docker image build -t cos_dev_env .
```

* To launch a shell in the docker container:

```bash
docker container run -v /home/<rest of absolute path>/composite:/root/composite -it -w=/root cos_dev_env zsh
```

* To build composite from scratch and run kernel tests in a single command:

```bash
docker container run -v /home/<rest of absolute path>/composite:/root/composite -it -w=/root/composite/src cos_dev_env bash -c "make && make run RUNSCRIPT=kernel_tests.sh run"
```

## Explanation

Docker is a containerization platform.  What that means is that it emulates the
behavior of an operating system, as observed by a user level application.
This is in contrast to full virtualization which emulates the behavior of a
machine as observed by the software (OS and userlevel).

Composite relies on a fair number of external packages during its build process
and emulated testing.  To minimized dependency hell, we have generally
converged on ubuntu 18.04 as a development platform.

Docker can assist by providing a reliable and deterministic build and testing
environment.  For those who do not with to run ubuntu on their personal
machines, docker can provide a better user experience than virtualbox or other
virtual machines.  For those who run ubuntu or other linux distributions,
docker can provide determinism in the build process, and isolation.  Packages
and environment details are totally separate.

## Use Cases

This dockerfile can act as a containerized buildsystem, or as a containerized
development environment.  You can edit source code from within the container
or your host machine.  

## Docker Flags and Commands

Docker is not difficult to learn from the online documentation.  However
if you only need to use docker as your build environment, you might as well
just memorize or set aliases for whichever command you use.

 * `docker image build -t cos_dev_env .`
 	This should be run from the same directory as `Dockerfile`.
	Note the trailing `.` which specifies the current directory.
	This command build a container with the tag `cos_dev_env`
 * `docker container run [options] cos_dev_env [command]`
 	Runs [command] in cos_dev_env
 * `docker container run -v <absolute host path>:/root/composite -w=/root/composite -it cos_dev_env zsh`
 	Mounts <absolute host path> at guest location /root/composite, and
	then runs zsh from the directory /root/composite. 
	`-it is necessicary for interactive terminal buffers required by bash or zsh
 * `docker container run -v /home/<rest of path>/composite:/root/composite -it -w=/root/composite/src cos_dev_env bash -c "make init && make && make run RUNSCRIPT=kernel_tests.sh run"`
 	Run the entire build process from scratch
 * `--ncpus=?`
 	Can be used on any of the docker run commands to set the number of cpus
