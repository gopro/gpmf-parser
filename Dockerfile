FROM fuzzers/afl:2.52

RUN apt-get update
RUN apt install -y build-essential wget git clang cmake
RUN git clone https://github.com/gopro/gpmf-parser.git
WORKDIR /gpmf-parser
RUN cmake -DBUILD_SHARED_LIBS=1 .
RUN make
RUN make install

WORKDIR ./demo

RUN afl-clang -g -c GPMF_demo.c
RUN afl-clang -g -c GPMF_mp4reader.c
RUN afl-clang -g -c GPMF_print.c
RUN afl-clang -g -c ../GPMF_parser.c
RUN afl-clang -g -c ../GPMF_utils.c
RUN afl-clang -o gpmfdemo GPMF_demo.o GPMF_parser.o GPMF_utils.o GPMF_mp4reader.o GPMF_print.o
RUN cp gpmfdemo /

# The samples are too big to be used as AFL testcases,
# need to fetch smaller (<1MB) mp4 files and add the GPMF structure.

# Smaller testcases (more found at the fuzzing-corpus repo below)
RUN wget https://www.sample-videos.com/video123/mp4/240/big_buck_bunny_240p_1mb.mp4
RUN mv big_buck_bunny_240p_1mb.mp4 sample1.mp4
RUN wget https://github.com/strongcourage/fuzzing-corpus/blob/master/mp4/mozilla/MPEG4.mp4 
RUN mkdir -p /in
RUN mv *.mp4 /in

# Add GPMF structure
WORKDIR /
RUN git clone https://github.com/gopro/gpmf-write.git
WORKDIR /gpmf-write
RUN cmake .
RUN make
RUN make install
RUN ./gpmf-writer /in/sample1.mp4
RUN ./gpmf-writer /in/MPEG4.mp4
WORKDIR /
WORKDIR /gpmf-parser
WORKDIR ./demo

# Fuzz
ENTRYPOINT ["afl-fuzz", "-i", "/in", "-o", "/out"]
CMD ["/gpmfdemo", "@@"] 
