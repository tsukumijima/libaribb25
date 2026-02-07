ARG IMAGE
FROM ${IMAGE}
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential ca-certificates libpcsclite-dev pkg-config wget
RUN CMAKE_ARCH=$(uname -m) && \
	wget https://github.com/Kitware/CMake/releases/download/v3.27.4/cmake-3.27.4-linux-${CMAKE_ARCH}.sh && \
	sh ./cmake-3.27.4-linux-${CMAKE_ARCH}.sh --prefix=/usr/local --skip-license && \
	rm ./cmake-3.27.4-linux-${CMAKE_ARCH}.sh
