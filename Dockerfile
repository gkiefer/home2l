# This file is part of the Home2L project.
#
# (C) 2015-2021 Gundolf Kiefer
#
# Home2L is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Home2L is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Home2L. If not, see <https://www.gnu.org/licenses/>.


# Dockerfile for a demo Home2L installation, to be used as a template for
# own installations.





#################### Usage Infos ###############################################


# Building the image locally:
#   $ docker build --build-arg BUILD_VERSION=test -t gkiefer/home2l:test .
#
# "testing" can be replaced by an arbitrary build version.
# '--build-arg BUILD_VERSION=...' can be ommitted when building a git repository,
# in which case the build version is determined by git.


# To run the image:
#     $ xhost +local:
#     $ docker run -ti --rm --tmpfs /tmp --name home2l-showcase --hostname home2l-showcase \
#         -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix --device /dev/snd \
#         gkiefer/home2l


# To start a new shell in an existing container:
#     $ docker exec -ti home2l-showcase bash
# or
#     $ alias DOCKER="docker exec -ti home2l-showcase"
#     ...
#     $ DOCKER bash


# To start a new root shell in the running container:
#     $ alias DOCKER_ROOT="docker exec -ti --user=root home2l-showcase"
#     ...
#     $ DOCKER_ROOT bash
#
# In both cases, 'bash' can be replaced by any other command, e.g.:
#     $ DOCKER home2l shell       # runs a Home2L shell


# For certain features, additional Debian packages may be needed.
#
# * For changing and re-compiling the floorplan:
#     $ DOCKER_ROOT apt-get install imagemagick inkscape
#
# * For testing X11 (xeyes etc.):
#     $ DOCKER_ROOT apt-get install x11-apps
#
# * For programming Brownies:
#     $ DOCKER_ROOT apt-get install avrdude


# Supplying music files for the audio player:
#     $ docker cp <some *.mp3 or *.flac directory>  home2l-showcase:/var/opt/home2l/mpd/music/
#
#   Caution: To use the audio player, all programs using sound on the *host* must be closed!





#################### Configuration #############################################


# Version of the Debian base image ...
ARG DEBIAN_VERSION=bullseye


# Build version as reported by the tools ...
ARG BUILD_VERSION=dev


# Phone settings ...

#   a) No phone ...
ARG WITH_PHONE=0

#   b) With phone (Linphone backend - presently unmaintained) ...
#ARG WITH_PHONE=1
#ARG PHONE_LIB=linphone
#ARG PHONE_DEBS_BUILD=liblinphone-dev
#ARG PHONE_DEBS_RUN=liblinphone9





#################### Stage 1: Building #########################################


FROM debian:$DEBIAN_VERSION AS builder

ARG BUILD_VERSION

ARG WITH_PHONE
ARG PHONE_LIB
ARG PHONE_DEBS_BUILD


# Install packages ...
RUN sed -i.bak 's#main\$#main contrib\$#' /etc/apt/sources.list && \
    apt-get update && \
    apt-get -y --no-install-recommends install \
      make g++ git \
      python3 swig python3-dev libreadline-dev \
      libsdl2-dev libsdl2-ttf-dev \
      gettext imagemagick inkscape \
      libmosquitto-dev \
      libmpdclient-dev \
      gcc-avr avr-libc \
      $PHONE_DEBS_BUILD


# Copy source tree, compile and install ...
WORKDIR /tmp/src
COPY . .
RUN make -j8 CFG=demo WITH_PHONE=$WITH_PHONE PHONE_LIB=$PHONE_LIB BUILD_VERSION=$BUILD_VERSION install && \
    mkdir -p /var/opt/home2l && cp -a doc/showcase/var/* /var/opt/home2l/





#################### Stage 2: Runnable Demo Image ##############################


FROM debian:$DEBIAN_VERSION AS showcase

ARG PHONE_DEBS_RUN


# Install packages ...
RUN sed -i.bak 's#main\$#main contrib\$#' /etc/apt/sources.list && \
    apt-get update && \
    apt-get -y --no-install-recommends install \
      nano less procps psmisc \
      python3 curl bsdmainutils \
      libsdl2-2.0-0 libsdl2-ttf-2.0-0 \
      mosquitto mosquitto-clients \
      libmpdclient2 mpd mpc alsa-utils ca-certificates \
      net-tools remind patch \
      i2c-tools \
      $PHONE_DEBS_RUN \
    && apt-get clean


# Copy build output products ...
COPY --from=builder /opt/home2l /opt/home2l
COPY --from=builder /var/opt/home2l /var/opt/home2l
#COPY --from=builder /tmp/src /opt/src


# Setup Home2L ...
#   Membership in group 'audio' is required for audio output (music player, phone).
#   Membership in groups 'i2c' and 'dialout' is required for the 'Brownies' tutorial.
RUN adduser --uid=5000 --disabled-password --gecos "User for auto-started Home2L instances" home2l && \
    adduser home2l audio && \
    adduser home2l i2c && \
    adduser home2l dialout && \
    chown -R home2l.home2l /var/opt/home2l && \
    mkdir -p /run/mosquitto && \
    chown home2l.root /var/lib/mosquitto /var/log/mosquitto /run/mosquitto && \
    /opt/home2l/bin/home2l-install -y -i && \
    echo "export PATH=\$PATH:/opt/home2l/bin:/opt/home2l/bin/`dpkg --print-architecture`" > /home/home2l/.bashrc


# Setup image ...
USER home2l
WORKDIR /opt/home2l/etc
ENV HOME2L_ROOT /opt/home2l
VOLUME $HOME2L_ROOT/etc
VOLUME /var/opt/home2l


# Run command ...
CMD home2l demo rundock
