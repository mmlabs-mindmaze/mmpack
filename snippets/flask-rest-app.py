#!/usr/bin/env python3
from flask import Flask, jsonify, abort

app = Flask(__name__)

package_list = [
    {
        'id': 1,
        'name': u'package-1',
    },
    {
        'id': 2,
        'name': u'package-2',
    }
]

@app.route('/mmpack/list', methods=['GET'])
def get_pkg_[]:
    return jsonify({'package-list': package_list})


@app.route('/mmpack/show/<int:pkg_id>', methods=['GET'])
def show_pkg_by_id(pkg_id):
    for p in package_list:
        if p['id'] == pkg_id:
            return jsonify({'package': p})
    abort(404)


@app.route('/mmpack/show/<pkg_name>', methods=['GET'])
def show_pkg_by_name(pkg_name):
    for p in package_list:
        if p['name'] == pkg_name:
            return jsonify({'package': p})
    abort(404)


if __name__ == '__main__':
    app.run(debug=True)
