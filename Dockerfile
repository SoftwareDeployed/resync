FROM debian:12-slim

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

RUN npm install --global pnpm

RUN useradd --create-home --shell /bin/bash opam

USER opam
WORKDIR /workspace
SHELL ["/bin/bash", "-lc"]

RUN opam init --bare --disable-sandboxing --yes
