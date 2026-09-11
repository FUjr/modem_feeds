FROM node:22-bookworm-slim

RUN apt-get update \
	&& apt-get install -y --no-install-recommends chromium ca-certificates fonts-liberation \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /work
COPY package.json package-lock.json ./
RUN npm ci --omit=optional

COPY playwright.config.js ./
COPY specs ./specs

ENTRYPOINT ["npx", "playwright", "test"]
