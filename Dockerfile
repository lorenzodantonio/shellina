FROM gcc:4.9 AS builder
COPY . /usr/src/shell
WORKDIR /usr/src/shell
RUN gcc -Wall -Wextra -Wpedantic -Werror -std=c99 *.c -o shell

FROM debian:stretch-slim
WORKDIR /root/
COPY --from=builder /usr/src/shell/shell .
CMD ["./shell"]