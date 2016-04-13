# TODO

* Support memcached.
* Support redis.
* Support http.
* Change from service us to request/s?

## Performance

* Should be able to use a circular buffer / queue to manage outstanding
  requests rather than new/delete (synthetic and memcache protocol both are
  FIFO ordering).
* Size accumulator on startup to avoid resizing during measurement.

