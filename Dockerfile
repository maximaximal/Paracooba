FROM public.ecr.aws/ubuntu/ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && apt-get install libboost-system-dev libasio-dev libboost-program-options-dev libboost-iostreams-dev libboost-coroutine-dev libboost-log-dev pkg-config cmake g++ make git awscli iproute2 iputils-ping -y
ADD . paracooba
RUN cd paracooba && mkdir build && cd build && cmake .. -DENABLE_TESTS=OFF -DSTATIC_BUILDS=ON -DCMAKE_BUILD_TYPE=Release && make -j2
EXPOSE 18001/tcp
ENTRYPOINT ["paracooba/aws-run.sh"]
CMD []
