FROM python:3.12-slim-bookworm AS native
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /source
COPY CMakeLists.txt CMakePresets.json LICENSE THIRD_PARTY_NOTICES.txt ./
COPY src ./src
COPY tests ./tests
COPY tools/*.c ./tools/
COPY tools/coop ./tools/coop
RUN cmake -S . -B build -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target coop_bridge_tests --parallel 2 \
    && ctest --test-dir build --output-on-failure -R '^shared_carriage_'

FROM python:3.12-slim-bookworm
WORKDIR /app
COPY tools/coop/requirements.txt ./requirements.txt
RUN pip install --no-cache-dir -r requirements.txt
COPY --from=native /source/build/libcrownless_coop.so ./libcrownless_coop.so
COPY tools/coop ./tools/coop
COPY web/coop ./web/coop
COPY out/build/web/site ./game
ARG CROWNLESS_REVISION=development
ENV CROWNLESS_REVISION=$CROWNLESS_REVISION PYTHONUNBUFFERED=1
EXPOSE 8787
CMD ["python", "tools/coop/host.py"]
