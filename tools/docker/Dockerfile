FROM ubuntu:18.04
RUN dpkg --add-architecture i386 &&\
	apt-get -y update &&\
	apt-get -y install \
		htop \
		zsh \
		python3 \
		git \
		wget \
		bc \
		gcc-arm-none-eabi \
		g++-multilib \
		gcc-multilib \
		libssl-dev \
		flex \
		bison \
		binutils-dev \
		binutils-multiarch-dev \
		zlib1g:i386 \
		libbinutils:i386 \
		qemu-kvm \
		cmake \
		build-essential \
		xorriso \
		curl \
		python \
		tmux \
		vim &&\
	chsh -s /bin/zsh root &&\
	wget https://github.com/robbyrussell/oh-my-zsh/raw/master/tools/install.sh -O - | zsh || true
RUN cd tmp &&\
	ls &&\
	apt-get download binutils-dev:i386 &&\
	ls &&\
	ar x *.deb &&\
	ls &&\
	tar -xvf data.tar.* &&\
	cp -d usr/lib/i386-linux-gnu/lib* /usr/lib/i386-linux-gnu/ &&\
	rm *.deb &&\
	cd ..
