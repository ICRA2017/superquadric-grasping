FROM ubuntu:16.04

RUN apt-get update && apt-get install -y \
	cmake libace-dev \
	&& rm -rf /var/lib/apt/lists

RUN apt-get update && apt-get install -y \
	git g++ \
	&& rm -rf /var/lib/apt/lists

RUN echo "deb http://www.icub.org/ubuntu xenial contrib/science" > /etc/apt/sources.list.d/icub.list \
	&& gpg --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 57A5ACB6110576A6

RUN apt-get update && apt-get install -y --allow-unauthenticated \
	icub \
	&& rm -rf /var/lib/apt/lists

RUN git clone https://github.com/robotology/yarp \
	&& cd yarp && mkdir build && cd build && cmake .. && make install

RUN  git clone https://github.com/robotology/icub-contrib-common.git \
	&& cd icub-contrib-common && mkdir build && cd build \
	&& cmake .. && make install

RUN git clone https://github.com/ICRA2017/superquadric-grasping.git \
	&& cd superquadric-grasping \
	&& mkdir build && cd build \
	&& cmake .. && make install
