
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y g++ make

WORKDIR /app

COPY . .

RUN make

EXPOSE 10000

CMD ["sh", "-c", "./heritage serve data/ndli_sample.csv ${PORT:-10000}"]
