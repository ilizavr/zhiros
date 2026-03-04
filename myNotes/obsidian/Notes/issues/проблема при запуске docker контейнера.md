
# смысл проблемы

смысл проблемы заключается в том что при вводе команды:
```bash
docker build .  -t hello
```

возникает проблема:
```bash
DEPRECATED: The legacy builder is deprecated and will be removed in a future release.
Install the buildx component to build images with BuildKit:
https://docs.docker.com/go/buildx/

Sending build context to Docker daemon  3.072kB
Step 1/5 : FROM golang:alpine
Get "https://registry-1.docker.io/v2/": dial tcp: lookup registry-1.docker.io on [::1]:53: read udp [::1]:43753->[::1]:53: read: connection refused
```

или при запуске контейнера   командой:
```bash
docker run  "имя образа"
```

возникает проблема:
```bash
Unable to find image 'hello-world:latest' locally
docker: Error response from daemon: Get "https://registry-1.docker.io/v2/": dial tcp: lookup registry-1.docker.io on [::1]:53: read udp [::1]:51871->[::1]:53: read: connection refused

Run 'docker run --help' for more information

```

# возможное решение
возможно ошибка в том что manjaro имеет какой то stub сервер, и он потенциально может иметь влияние на загрузку контенеров и образов докера

# сообщение для  stack over flow(если очень надо)

Not needed. Problem Solved