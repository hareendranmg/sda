extern crate serialport;

use std::time::Duration;

fn main() {
    let mut port = serialport::new("/dev/ttyXR0", 2_000_000)
        .timeout(Duration::from_micros(100))
        .open()
        .expect("Failed to open port");

    // writehex command of 0F to the port
    let buf: Vec<u8> = vec![0x0F];
    port.write(&buf).expect("Write failed");

    // delay for 10 microseconds
    std::thread::sleep(Duration::from_micros(10));
    // wait for 4microseconds

    // read 22 byte response from the port
    let mut buf: Vec<u8> = vec![0; 22];
    port.read(buf.as_mut_slice()).expect("Read failed");
    // convert the response to hex
    let hex = hex::encode(buf);
    println!("Response: {}", hex);
}
