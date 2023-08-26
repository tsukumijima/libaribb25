ARG IMAGE
FROM ${IMAGE}
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential ca-certificates libpcsclite-dev pkg-config wget
RUN wget https://github.com/Kitware/CMake/releases/download/v3.27.4/cmake-3.27.4-linux-aarch64.sh && \
	sh ./cmake-3.27.4-linux-aarch64.sh --prefix=/usr/local --skip-license && \
	rm ./cmake-3.27.4-linux-aarch64.sh
