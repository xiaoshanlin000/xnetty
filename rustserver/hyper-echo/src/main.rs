use hyper::body::Incoming;
use hyper::server::conn::http1;
use hyper::service::service_fn;
use hyper::{Request, Response};
use hyper_util::rt::TokioIo;
use std::net::SocketAddr;

async fn handle(_req: Request<Incoming>) -> Result<Response<String>, hyper::Error> {
    Ok(Response::new("Hello, World!".to_string()))
}

#[tokio::main(flavor = "multi_thread")]
async fn main() {
    let addr: SocketAddr = ([127, 0, 0, 1], 19998).into();
    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    println!("hyper-echo on http://{}/", addr);

    loop {
        let (stream, _) = listener.accept().await.unwrap();
        tokio::task::spawn(async move {
            let io = TokioIo::new(stream);
            if let Err(e) = http1::Builder::new()
                .serve_connection(io, service_fn(handle))
                .await
            {
                eprintln!("hyper: {}", e);
            }
        });
    }
}
