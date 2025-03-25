FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y libqt5core5a libqt5network5
WORKDIR /app
COPY ChatServer /app/
EXPOSE 12345
CMD ["/project1/ChatServer"]
