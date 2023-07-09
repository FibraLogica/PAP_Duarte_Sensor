from flask import Flask, request, jsonify
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime

# Cria uma app Flask
app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = 'mysql://myadmin:Olamundo123@localhost/express'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
db = SQLAlchemy(app)


# Cria a class User para guardar dados sobre cada utilizador, como o id e o código do fingerprint.
class User(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    fingerprint_code = db.Column(db.String(120), unique=True, nullable=False)
    access_history = db.relationship('AccessHistory', backref='user', lazy=True)

# Declara um metodo "to_dict" onde retorna toda a informação sobre si.
    def to_dict(self):
        return {
            'id': self.id,
            'fingerprint_code': self.fingerprint_code,
            'access_history': [access.date.strftime('%Y-%m-%d %H:%M:%S') for access in self.access_history]
        }

# Cria a classe AccessHistory, para poder registrar cada acesso que aconteceu.
class AccessHistory(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=False)
    date = db.Column(db.DateTime, nullable=False, default=datetime.utcnow)

    # Declara um metodo "to_dict" onde retorna toda a informação sobre si.
    def to_dict(self):
        return {
            'id': self.id,
            'user_id': self.user_id,
            'date': self.date.strftime('%Y-%m-%d %H:%M:%S')
        }

@app.route('/create_user', methods=['POST'])
def create_user():
    try:
        # Recebe o json do request.
        data = request.get_json()
        fingerprint_code = data.get('fingerprint_code')
        
        # Verificar se o código de impressão digital já existe
        existing_user = User.query.filter_by(fingerprint_code=fingerprint_code).first()
        if existing_user:
            return jsonify({'message': 'Código de impressão digital já registado!', 'user': existing_user.to_dict()}), 409
        
        # Cria um utilizador com o código.
        user = User(fingerprint_code=fingerprint_code)
        # Adiciona o utilizador á sessão.
        db.session.add(user)
        # Aplica mudanças.
        db.session.commit()
        # Retorna um print.
        return jsonify({'message': 'Utilizador criado', 'user': user.to_dict()}), 201
    except Exception as e:
        # Log da exceção
        print(str(e))
        return jsonify({'message': 'Ocorreu um erro ao criar o utilizador'}), 500


@app.route('/unlock', methods=['POST'])
def unlock_door():
    print(request.json)
    # Cria um novo acesso no histórico
    user_id = request.json.get('user_id')
    user = db.session.get(User, user_id)
    
    if not user:
        return jsonify({'message': 'Utilizador não encontrado'}), 404
    
    access = AccessHistory(user_id=user.id)
    db.session.add(access)
    db.session.commit()
    
    return jsonify({'message': 'Porta desbloqueada! Bem-vindo', 'user': user.to_dict()}), 200

if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(host='0.0.0.0', debug=True)