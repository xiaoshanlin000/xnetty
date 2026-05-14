Bun.serve({
    port: 20000,
    fetch(_req) {
        return new Response("Hello, World!");
    },
});

console.log("bun-native on http://127.0.0.1:20000/");
