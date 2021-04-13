FROM gcc:10.1.0

COPY . gpmf-parser
WORKDIR gpmf-parser/demo
RUN make
WORKDIR ..
ENTRYPOINT [ "./run.sh" ]
