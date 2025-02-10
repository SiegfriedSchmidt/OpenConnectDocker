# === Build Stage ===
FROM debian:bullseye-slim AS build
ENV DEBIAN_FRONTEND=noninteractive

# Install basic build tools and required libraries plus extra packages to satisfy tests/build:
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    autoconf \
    automake \
    libtool \
    git \
    libgnutls28-dev \
    libev-dev \
    ipcalc-ng \
    protobuf-c-compiler \
    libprotobuf-c-dev \
    gperf \
    libreadline-dev \
 && rm -rf /var/lib/apt/lists/*

# Copy the entire repository (your fork) into the build stage.
WORKDIR /src
COPY . .

# Regenerate build scripts, configure, compile, and install into /install.
RUN autoreconf -fvi && \
    ./configure && \
    make && \
    make install DESTDIR=/install

# === Final Stage ===
FROM debian:bullseye-slim
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime libraries needed by ocserv.
RUN apt-get update && apt-get install -y --no-install-recommends \
    gnutls-bin \
    libev4 \
    libnl-3-200 \
    libpam0g \
    liblz4-1 \
    libseccomp2 \
    libreadline8 \
    ca-certificates \
    libprotobuf-c-dev \
 && rm -rf /var/lib/apt/lists/*

# Copy the compiled files from the build stage.
COPY --from=build /install/ /

# Run ocserv in the foreground using the configuration file from /etc/ocserv/ocserv.conf.
CMD ["/usr/local/sbin/ocserv", "--foreground", "--config", "/etc/ocserv/ocserv.conf"]
