FROM ubuntu:18.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && apt-get install libboost-all-dev cmake g++ make git awscli -y
ADD . paracooba
RUN cd paracooba && mkdir build && cd build && cmake .. && make
EXPOSE 18001
ENTRYPOINT ["paracooba/aws-run.sh"]
CMD []
