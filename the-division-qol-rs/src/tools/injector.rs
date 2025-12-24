use crate::tools::mappings;
use libmem::*;
use std::env;
use std::io::{self, Write};

pub fn boot() -> () {
    let process = match find_process(mappings::DIVISION_PROC) {
        Some(p) => p,
        None => {
            println!("Process '{}' not found. Exiting.", mappings::DIVISION_PROC);
            await_for_user_interaction();
            return;
        }
    };

    println!("Found process: {} (PID: {})", process.name, process.pid);

    let dll_path = env::current_dir();

    match dll_path {
        Ok(mut path) => {
            path.push("dlls");
            path.push(mappings::DLL_NAME);

            let dll_str = path.to_str();
            if dll_str.is_none() {
                println!("Path contains invalid chars.");
                await_for_user_interaction();
                return;
            }

            println!("Targeting: {:?}", dll_str);

            match load_module_ex(&process, dll_str.unwrap()) {
                Some(m) => println!("Successfully injected! Base address: 0x{:X}", m.base),
                None => {
                    println!("FAILED to inject.");
                    println!("Possible reasons:");
                    println!("- You didn't run as Administrator (Right-click your IDE/EXE)");
                    println!("- The DLL is 32-bit but the game is 64-bit (or vice versa)");
                    println!("- Anti-cheat (EAC) blocked the LoadLibrary call");
                    await_for_user_interaction();
                    return;
                }
            }
        }
        Err(e) => {
            println!(
                "Unable to grab the current path to handle DLL. Err: {:?}",
                e
            );
            await_for_user_interaction();
            return;
        }
    }
}

fn await_for_user_interaction() {
    print!("Press ENTER to exit...");
    io::stdout().flush().unwrap();
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
}
