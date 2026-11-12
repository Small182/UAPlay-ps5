FROM node:22-bookworm-slim AS builder
RUN apt-get update && apt-get install -y --no-install-recommends git gcc libc6-dev pkg-config libcurl4-openssl-dev ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /build
ARG UPSTREAM_COMMIT=21b87a3673cd63eb4c11301d62380a6fa7b38cd8
RUN git clone https://github.com/Small182/UAPlay-nx.git upstream && \
    git -C upstream checkout --detach "$UPSTREAM_COMMIT"
COPY native/uaplay_bridge.c /build/uaplay_bridge.c
RUN cc -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L \
    -I/build/upstream/source /build/uaplay_bridge.c /build/upstream/source/catalog.c \
    $(pkg-config --cflags --libs libcurl) -o /build/uaplay_bridge

FROM node:22-bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libcurl4 ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY server.js /app/server.js
COPY --from=builder /build/uaplay_bridge /usr/local/bin/uaplay_bridge
ENV NODE_ENV=production PORT=7000 BRIDGE_PATH=/usr/local/bin/uaplay_bridge
EXPOSE 7000
USER node
CMD ["node", "/app/server.js"]
