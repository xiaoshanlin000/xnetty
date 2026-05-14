import { Elysia } from "elysia";

new Elysia()
    .get("/", () => "Hello, World!")
    .listen(20001);

console.log("elysia on http://127.0.0.1:20001/");
