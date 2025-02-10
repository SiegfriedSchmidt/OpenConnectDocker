# === Build Stage ===
FROM debian:bullseye-slim AS build
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    autoconf \
    automake \
    libtool \
    git \
    libgnutls28-dev \
    libev-dev \
    libnl-route-3-dev \
    libpam0g-dev \
    libseccomp-dev \          
    ipcalc-ng \
    protobuf-c-compiler \
    libprotobuf-c-dev \
    gperf \
    libreadline-dev \
    gnutls-bin \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN autoreconf -fvi && \
    ./configure \
      --with-seccomp \     
      --enable-linux-namespaces && \
    make && \
    make install DESTDIR=/install

# === Final Stage ===
FROM debian:bullseye-slim
ENV DEBIAN_FRONTEND=noninterciall

RUN apt-get update && apt-get install -y --no-install-recommends \
    libev4 \
    libnl-3-200 \
    libnl-route-3-200 \
    libpam0g \
    libseccomp2 \           
    libreadline8 \
    gnutls-bin \
    iptables \
    ca-certificates \
    libprotobuf-c1 \
 && rm -rf /var/lib/apt/lists/*

COPY --from=build /install/ /

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/local/sbin/ocserv", "--foreground", "--config", "/etc/ocserv/ocserv.conf"]