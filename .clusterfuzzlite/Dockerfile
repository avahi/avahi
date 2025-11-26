FROM gcr.io/oss-fuzz-base/base-builder:v1
ENV MERGE_WITH_OSS_FUZZ_CORPORA=yes
COPY . $SRC/avahi
WORKDIR $SRC/avahi
COPY fuzz/oss-fuzz.sh $SRC/build.sh
