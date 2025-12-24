use tools::injector;

mod tools;

fn main() {
    println!(
        "+--------------------------------------+\n\
         | NetSSH - Quality of Life Improvements|\n\
         | (c) 2025 NetSSH                      |\n\
         +-------------------------------------+"
    );

    println!(
        "Ensure that:\n\
         - You ARE NOT using FULLSCREEN Mode\n\
         - You ARE NOT on Directx12\n"
    );
    injector::boot();
}
