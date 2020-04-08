FROM ubuntu:18.04
RUN apt-get update && apt-get upgrade -y
RUN apt-get install libboost-all-dev cmake g++ make git -y
ADD . paracooba
RUN cd paracooba && mkdir build && cd build && cmake .. && make
EXPOSE 18001
ENTRYPOINT ["paracooba/aws-run.sh"]
CMD []
