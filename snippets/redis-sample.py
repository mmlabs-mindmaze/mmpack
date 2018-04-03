import redis
import json

r = redis.Redis('localhost')

key = 'key'
value = { 'k1' : 'value1', 'k2': 'value2'}

r.set(key, json.dumps(value))

print("r.exists(key) = ", r.exists(key))
print("r.get(key) = ", r.get(key))
print("r.get('invalid') = ", r.get('invalid'))

value2 = json.loads(r.get(key).decode('utf-8'))
value2['key3'] = 'value3'

if r.exists(key):
    r.set(key, json.dumps(value2))

print("r.get(key) = ", r.get(key))
