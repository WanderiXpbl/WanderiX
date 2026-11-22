# Builds and runs the WanderiX web server on a hosting service (Render, or any Docker host).
# Step 1: compile the C++ server on Linux.
FROM gcc:13 AS build
WORKDIR /app
COPY . .
RUN g++ -std=c++17 -O2 -pthread -static-libstdc++ -static-libgcc server.cpp -o wanderix-server

# Step 2: a small image with just the program, the data and the web page.
FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /app/wanderix-server ./
COPY data ./data
COPY web ./web
ENV PORT=8080
EXPOSE 8080
CMD ["./wanderix-server"]
