FROM debian:13-slim

ENV DEBIAN_FRONTEND=noninteractive \
    OPAMROOT=/home/opam/.opam

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    bubblewrap \
    ca-certificates \
    curl \
    git \
    libev-dev \
    libffi-dev \
    libgmp-dev \
    libicu-dev \
    libpq-dev \
    m4 \
    nodejs \
    npm \
    opam \
    patch \
    pkg-config \
    rsync \
    unzip \
    && rm -rf /var/lib/apt/lists/*

  ARG WATCHEXEC_VERSION=2.8.0
  RUN curl -fsSL "https://github.com/watchexec/watchexec/releases/download/v${WATCHEXEC_VERSION}/watchexec-${WATCHEXEC_VERSION}-x86_64-unknown-linux-musl.tar.xz" -o /tmp/watchexec.tar.xz \
      && tar -xJf /tmp/watchexec.tar.xz -C /usr/local/bin --strip-components=1 \
      && rm /tmp/watchexec.tar.xz

RUN npm install --global pnpm

RUN useradd --create-home --shell /bin/bash opam

USER opam
WORKDIR /workspace
SHELL ["/bin/bash", "-lc"]

RUN opam init --bare --disable-sandboxing --yes
