# === Build Stage ===
FROM debian:bullseye-slim AS build
ENV DEBIAN_FRONTEND=noninteractive

# Install required build dependencies
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
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
# Copy the entire forked repository into the build container.
COPY . .

# Regenerate build scripts and compile ocserv (using the instructions from the official README)
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
 && rm -rf /var/lib/apt/lists/*

# Copy the compiled ocserv files from the build stage.
COPY --from=build /install/ /

CMD ["/usr/local/sbin/ocserv", "--foreground", "--config", "/etc/ocserv/ocserv.conf"]
