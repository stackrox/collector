ARG BUILD_DIR=/build
ARG CMAKE_BUILD_DIR=${BUILD_DIR}/cmake-build


FROM registry.access.redhat.com/ubi9/ubi:latest@sha256:cecb1cde7bda7c8165ae27841c2335667f8a3665a349c0d051329c61660a496c AS builder

RUN cat /cachi2/cachi2.env
