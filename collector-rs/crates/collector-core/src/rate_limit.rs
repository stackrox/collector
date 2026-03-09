use std::collections::HashMap;
use std::time::{Duration, Instant};

const DEFAULT_BURST: u32 = 10;
const DEFAULT_REFILL_INTERVAL: Duration = Duration::from_secs(30 * 60);
const DEFAULT_MAX_ENTRIES: usize = 65536;

struct TokenBucket {
    tokens: u32,
    last_refill: Instant,
}

pub struct RateLimitCache {
    buckets: HashMap<String, TokenBucket>,
    burst: u32,
    refill_interval: Duration,
    max_entries: usize,
}

impl RateLimitCache {
    pub fn new() -> Self {
        Self {
            buckets: HashMap::new(),
            burst: DEFAULT_BURST,
            refill_interval: DEFAULT_REFILL_INTERVAL,
            max_entries: DEFAULT_MAX_ENTRIES,
        }
    }

    pub fn with_config(burst: u32, refill_interval: Duration, max_entries: usize) -> Self {
        Self {
            buckets: HashMap::new(),
            burst,
            refill_interval,
            max_entries,
        }
    }

    pub fn allow(&mut self, key: &str) -> bool {
        self.allow_at(key, Instant::now())
    }

    fn allow_at(&mut self, key: &str, now: Instant) -> bool {
        if let Some(bucket) = self.buckets.get_mut(key) {
            if now.duration_since(bucket.last_refill) >= self.refill_interval {
                bucket.tokens = self.burst;
                bucket.last_refill = now;
            }
            if bucket.tokens > 0 {
                bucket.tokens -= 1;
                true
            } else {
                false
            }
        } else {
            if self.buckets.len() >= self.max_entries {
                return false;
            }
            self.buckets.insert(
                key.to_string(),
                TokenBucket {
                    tokens: self.burst - 1,
                    last_refill: now,
                },
            );
            true
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn allows_burst_then_blocks() {
        let mut cache = RateLimitCache::with_config(3, Duration::from_secs(60), 100);
        let now = Instant::now();

        assert!(cache.allow_at("key1", now));
        assert!(cache.allow_at("key1", now));
        assert!(cache.allow_at("key1", now));
        assert!(!cache.allow_at("key1", now));
        assert!(!cache.allow_at("key1", now));
    }

    #[test]
    fn different_keys_are_independent() {
        let mut cache = RateLimitCache::with_config(2, Duration::from_secs(60), 100);
        let now = Instant::now();

        assert!(cache.allow_at("a", now));
        assert!(cache.allow_at("a", now));
        assert!(!cache.allow_at("a", now));

        assert!(cache.allow_at("b", now));
        assert!(cache.allow_at("b", now));
        assert!(!cache.allow_at("b", now));
    }

    #[test]
    fn refills_after_interval() {
        let mut cache = RateLimitCache::with_config(2, Duration::from_secs(10), 100);
        let now = Instant::now();

        assert!(cache.allow_at("key", now));
        assert!(cache.allow_at("key", now));
        assert!(!cache.allow_at("key", now));

        let later = now + Duration::from_secs(11);
        assert!(cache.allow_at("key", later));
        assert!(cache.allow_at("key", later));
        assert!(!cache.allow_at("key", later));
    }

    #[test]
    fn respects_max_entries() {
        let mut cache = RateLimitCache::with_config(1, Duration::from_secs(60), 2);
        let now = Instant::now();

        assert!(cache.allow_at("a", now));
        assert!(cache.allow_at("b", now));
        assert!(!cache.allow_at("c", now));
    }
}
