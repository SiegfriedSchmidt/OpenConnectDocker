# ----- BUILD STAGE -----
FROM debian:stable-slim AS builder
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # git
    curl \
    ca-certificates \
    git \ 
    # dependencies that are not installed due to "--no-install-recommends"
    autoconf \
    automake \
    ipcalc-ng \
    # main dependencies
    build-essential \
    pkg-config \
    libgnutls28-dev \
    libev-dev \
    libreadline-dev \
    libseccomp-dev \
    protobuf-c-compiler \
    libprotobuf-c-dev \
    gperf \
    liblz4-dev \
&& update-ca-certificates \
&& rm -rf /var/lib/apt/lists/*

RUN git clone https://gitlab.com/openconnect/ocserv.git /src
WORKDIR /src

RUN autoreconf -fvi && \
    ./configure && \
    make && \
    make install DESTDIR=/install

# ----- FINAL STAGE -----
FROM debian:stable-slim
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # main deps
    gnutls-bin \
    libev4 \
    libprotobuf-c1 \
    libreadline8 \
    # minor deps (some of them already installed)
    liblz4-1 \
    libseccomp2 \
    nftables \
&& rm -rf /var/lib/apt/lists/*

COPY --from=builder /install/ /

COPY scripts/* /usr/local/bin/
RUN chmod +x /usr/local/bin/*.sh
ENV PATH="/usr/local/bin:${PATH}"

RUN groupadd -g 1001 ocserv && \
    useradd -u 1001 -g ocserv \
      --no-create-home \
      --home-dir /nonexistent \
      --shell /usr/sbin/nologin \
      ocserv

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/local/sbin/ocserv", "--foreground", "--config", "/ocserv.conf"]
