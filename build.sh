if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    cmake -B ./build -GNinja
    cmake --build build
elif [[ "$OSTYPE" == "darwin"* ]]; then
        docker build --tag epoll_server .; docker stop $(docker ps -q); docker run --rm -p 8080:8080 --ulimit nofile=262144:262144 -d --security-opt seccomp:unconfined epoll_server
else
    echo "Unsupported OS"
fi