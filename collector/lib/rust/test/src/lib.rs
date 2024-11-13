#[no_mangle]
pub extern "C" fn external_add(left: u64, right: u64) -> u64 {
    add(left, right)
}

fn add(left: u64, right: u64) -> u64 {
    left + right
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let result = add(2, 2);
        assert_eq!(result, 4);
    }
}
