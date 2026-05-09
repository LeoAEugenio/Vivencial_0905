#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

const int WIDTH = 800;
const int HEIGHT = 600;

enum class ModoTransformacao {
    TRANSLACAO,
    ROTACAO,
    ESCALA
};

enum class Eixo {
    X,
    Y,
    Z
};

struct Vertex {
    glm::vec3 position;
};

struct Objeto3D {
    std::vector<Vertex> vertices;

    GLuint VAO = 0;
    GLuint VBO = 0;

    glm::vec3 posicao = glm::vec3(0.0f);
    glm::vec3 rotacao = glm::vec3(0.0f);
    glm::vec3 escala = glm::vec3(1.0f);
};

std::vector<Objeto3D> objetos;
int objetoSelecionado = 0;

ModoTransformacao modoAtual = ModoTransformacao::TRANSLACAO;
Eixo eixoAtual = Eixo::X;

float velocidadeTranslacao = 0.1f;
float velocidadeRotacao = 5.0f;
float velocidadeEscala = 0.1f;

const char* vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core

out vec4 FragColor;

uniform bool selecionado;

void main()
{
    if (selecionado)
        FragColor = vec4(1.0, 0.75, 0.15, 1.0);
    else
        FragColor = vec4(0.65, 0.65, 0.65, 1.0);
}
)";

GLuint compilarShader(GLenum tipo, const char* codigo) {
    GLuint shader = glCreateShader(tipo);
    glShaderSource(shader, 1, &codigo, nullptr);
    glCompileShader(shader);

    int sucesso;
    char infoLog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &sucesso);

    if (!sucesso) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "Erro ao compilar shader:\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint criarProgramaShader() {
    GLuint vertexShader = compilarShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compilarShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint programa = glCreateProgram();
    glAttachShader(programa, vertexShader);
    glAttachShader(programa, fragmentShader);
    glLinkProgram(programa);

    int sucesso;
    char infoLog[512];

    glGetProgramiv(programa, GL_LINK_STATUS, &sucesso);

    if (!sucesso) {
        glGetProgramInfoLog(programa, 512, nullptr, infoLog);
        std::cout << "Erro ao linkar shader program:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return programa;
}

bool carregarOBJ(const std::string& caminho, Objeto3D& objeto) {
    std::ifstream arquivo(caminho);

    if (!arquivo.is_open()) {
        std::cout << "Erro ao abrir arquivo OBJ: " << caminho << std::endl;
        return false;
    }

    std::vector<glm::vec3> posicoes;
    std::string linha;

    while (std::getline(arquivo, linha)) {
        std::stringstream ss(linha);
        std::string tipo;
        ss >> tipo;

        if (tipo == "v") {
            glm::vec3 posicao;
            ss >> posicao.x >> posicao.y >> posicao.z;
            posicoes.push_back(posicao);
        }

        else if (tipo == "f") {
            std::vector<unsigned int> indicesFace;
            std::string verticeStr;

            while (ss >> verticeStr) {
                std::stringstream verticeSS(verticeStr);
                std::string indiceStr;

                std::getline(verticeSS, indiceStr, '/');

                if (!indiceStr.empty()) {
                    int indice = std::stoi(indiceStr);

                    if (indice < 0) {
                        indice = static_cast<int>(posicoes.size()) + indice + 1;
                    }

                    indicesFace.push_back(indice - 1);
                }
            }

            if (indicesFace.size() >= 3) {
                for (size_t i = 1; i < indicesFace.size() - 1; i++) {
                    Vertex v1;
                    Vertex v2;
                    Vertex v3;

                    v1.position = posicoes[indicesFace[0]];
                    v2.position = posicoes[indicesFace[i]];
                    v3.position = posicoes[indicesFace[i + 1]];

                    objeto.vertices.push_back(v1);
                    objeto.vertices.push_back(v2);
                    objeto.vertices.push_back(v3);
                }
            }
        }
    }

    arquivo.close();

    if (objeto.vertices.empty()) {
        std::cout << "O arquivo OBJ nao possui vertices validos: " << caminho << std::endl;
        return false;
    }

    glGenVertexArrays(1, &objeto.VAO);
    glGenBuffers(1, &objeto.VBO);

    glBindVertexArray(objeto.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, objeto.VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        objeto.vertices.size() * sizeof(Vertex),
        objeto.vertices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    std::cout << "OBJ carregado com sucesso: " << caminho << std::endl;

    return true;
}

glm::mat4 criarMatrizModelo(const Objeto3D& objeto) {
    glm::mat4 model = glm::mat4(1.0f);

    model = glm::translate(model, objeto.posicao);

    model = glm::rotate(
        model,
        glm::radians(objeto.rotacao.x),
        glm::vec3(1.0f, 0.0f, 0.0f)
    );

    model = glm::rotate(
        model,
        glm::radians(objeto.rotacao.y),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    model = glm::rotate(
        model,
        glm::radians(objeto.rotacao.z),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    model = glm::scale(model, objeto.escala);

    return model;
}

void aplicarTransformacao(Objeto3D& objeto, float valor) {
    if (modoAtual == ModoTransformacao::TRANSLACAO) {
        if (eixoAtual == Eixo::X)
            objeto.posicao.x += valor * velocidadeTranslacao;

        else if (eixoAtual == Eixo::Y)
            objeto.posicao.y += valor * velocidadeTranslacao;

        else if (eixoAtual == Eixo::Z)
            objeto.posicao.z += valor * velocidadeTranslacao;
    }

    else if (modoAtual == ModoTransformacao::ROTACAO) {
        if (eixoAtual == Eixo::X)
            objeto.rotacao.x += valor * velocidadeRotacao;

        else if (eixoAtual == Eixo::Y)
            objeto.rotacao.y += valor * velocidadeRotacao;

        else if (eixoAtual == Eixo::Z)
            objeto.rotacao.z += valor * velocidadeRotacao;
    }

    else if (modoAtual == ModoTransformacao::ESCALA) {
        if (eixoAtual == Eixo::X)
            objeto.escala.x += valor * velocidadeEscala;

        else if (eixoAtual == Eixo::Y)
            objeto.escala.y += valor * velocidadeEscala;

        else if (eixoAtual == Eixo::Z)
            objeto.escala.z += valor * velocidadeEscala;

        if (objeto.escala.x < 0.1f) objeto.escala.x = 0.1f;
        if (objeto.escala.y < 0.1f) objeto.escala.y = 0.1f;
        if (objeto.escala.z < 0.1f) objeto.escala.z = 0.1f;
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;

    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, true);
        return;
    }

    if (objetos.empty())
        return;

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        objetoSelecionado++;

        if (objetoSelecionado >= static_cast<int>(objetos.size())) {
            objetoSelecionado = 0;
        }

        std::cout << "Objeto selecionado: " << objetoSelecionado + 1 << std::endl;
        return;
    }

    Objeto3D& objeto = objetos[objetoSelecionado];

    if (key == GLFW_KEY_T) {
        modoAtual = ModoTransformacao::TRANSLACAO;
        std::cout << "Modo: Translacao" << std::endl;
        return;
    }

    if (key == GLFW_KEY_R) {
        modoAtual = ModoTransformacao::ROTACAO;
        std::cout << "Modo: Rotacao" << std::endl;
        return;
    }

    if (key == GLFW_KEY_C) {
        modoAtual = ModoTransformacao::ESCALA;
        std::cout << "Modo: Escala" << std::endl;
        return;
    }

    if (key == GLFW_KEY_X) {
        eixoAtual = Eixo::X;
        std::cout << "Eixo: X" << std::endl;
        return;
    }

    if (key == GLFW_KEY_Y) {
        eixoAtual = Eixo::Y;
        std::cout << "Eixo: Y" << std::endl;
        return;
    }

    if (key == GLFW_KEY_Z) {
        eixoAtual = Eixo::Z;
        std::cout << "Eixo: Z" << std::endl;
        return;
    }

    if (key == GLFW_KEY_RIGHT) {
        aplicarTransformacao(objeto, 1.0f);
        return;
    }

    if (key == GLFW_KEY_LEFT) {
        aplicarTransformacao(objeto, -1.0f);
        return;
    }

    if (key == GLFW_KEY_UP) {
        aplicarTransformacao(objeto, 1.0f);
        return;
    }

    if (key == GLFW_KEY_DOWN) {
        aplicarTransformacao(objeto, -1.0f);
        return;
    }

    if (modoAtual == ModoTransformacao::TRANSLACAO) {
        if (key == GLFW_KEY_A)
            objeto.posicao.x -= velocidadeTranslacao;

        else if (key == GLFW_KEY_D)
            objeto.posicao.x += velocidadeTranslacao;

        else if (key == GLFW_KEY_W)
            objeto.posicao.z -= velocidadeTranslacao;

        else if (key == GLFW_KEY_S)
            objeto.posicao.z += velocidadeTranslacao;

        else if (key == GLFW_KEY_Q)
            objeto.posicao.y -= velocidadeTranslacao;

        else if (key == GLFW_KEY_E)
            objeto.posicao.y += velocidadeTranslacao;
    }

    if (modoAtual == ModoTransformacao::ROTACAO) {
        if (key == GLFW_KEY_A)
            objeto.rotacao.y -= velocidadeRotacao;

        else if (key == GLFW_KEY_D)
            objeto.rotacao.y += velocidadeRotacao;

        else if (key == GLFW_KEY_W)
            objeto.rotacao.x -= velocidadeRotacao;

        else if (key == GLFW_KEY_S)
            objeto.rotacao.x += velocidadeRotacao;

        else if (key == GLFW_KEY_Q)
            objeto.rotacao.z -= velocidadeRotacao;

        else if (key == GLFW_KEY_E)
            objeto.rotacao.z += velocidadeRotacao;
    }

    if (modoAtual == ModoTransformacao::ESCALA) {
        if (key == GLFW_KEY_W || key == GLFW_KEY_D || key == GLFW_KEY_E) {
            objeto.escala += glm::vec3(velocidadeEscala);
        }

        else if (key == GLFW_KEY_S || key == GLFW_KEY_A || key == GLFW_KEY_Q) {
            objeto.escala -= glm::vec3(velocidadeEscala);
        }

        if (objeto.escala.x < 0.1f) objeto.escala.x = 0.1f;
        if (objeto.escala.y < 0.1f) objeto.escala.y = 0.1f;
        if (objeto.escala.z < 0.1f) objeto.escala.z = 0.1f;
    }
}

int main() {
    if (!glfwInit()) {
        std::cout << "Erro ao inicializar GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        WIDTH,
        HEIGHT,
        "Exercicio OBJ - Atividade Vivencial 09/05/2026",
        nullptr,
        nullptr
    );

    if (!window) {
        std::cout << "Erro ao criar janela GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Erro ao inicializar GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = criarProgramaShader();

    Objeto3D suzanne1;
    Objeto3D suzanne2;
    Objeto3D suzanne3;

    std::string caminhoOBJ = "../assets/Modelos3D/Suzanne.obj";

    bool carregou1 = carregarOBJ(caminhoOBJ, suzanne1);
    bool carregou2 = carregarOBJ(caminhoOBJ, suzanne2);
    bool carregou3 = carregarOBJ(caminhoOBJ, suzanne3);

    if (carregou1) {
        suzanne1.posicao = glm::vec3(-2.0f, 0.0f, 0.0f);
        suzanne1.escala = glm::vec3(0.8f);
        objetos.push_back(suzanne1);
    }

    if (carregou2) {
        suzanne2.posicao = glm::vec3(0.0f, 0.0f, 0.0f);
        suzanne2.escala = glm::vec3(0.8f);
        objetos.push_back(suzanne2);
    }

    if (carregou3) {
        suzanne3.posicao = glm::vec3(2.0f, 0.0f, 0.0f);
        suzanne3.escala = glm::vec3(0.8f);
        objetos.push_back(suzanne3);
    }

    if (objetos.empty()) {
        std::cout << "Nenhum objeto foi carregado. Verifique o caminho do arquivo OBJ." << std::endl;
        std::cout << "Caminho esperado: " << caminhoOBJ << std::endl;
    }

    std::cout << "Controles:" << std::endl;
    std::cout << "TAB - selecionar proximo objeto" << std::endl;
    std::cout << "T - modo translacao" << std::endl;
    std::cout << "R - modo rotacao" << std::endl;
    std::cout << "C - modo escala" << std::endl;
    std::cout << "X/Y/Z - selecionar eixo" << std::endl;
    std::cout << "Setas direita/esquerda - aplicar transformacao no eixo selecionado" << std::endl;
    std::cout << "Modo translacao: A/D eixo X, W/S eixo Z, Q/E eixo Y" << std::endl;
    std::cout << "Modo rotacao: A/D eixo Y, W/S eixo X, Q/E eixo Z" << std::endl;
    std::cout << "Modo escala: W/D/E aumenta, S/A/Q diminui" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 1.5f, 7.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
            0.1f,
            100.0f
        );

        GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
        GLuint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
        GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLuint selecionadoLoc = glGetUniformLocation(shaderProgram, "selecionado");

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        for (size_t i = 0; i < objetos.size(); i++) {
            glm::mat4 model = criarMatrizModelo(objetos[i]);

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(selecionadoLoc, i == static_cast<size_t>(objetoSelecionado));

            glBindVertexArray(objetos[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(objetos[i].vertices.size()));
            glBindVertexArray(0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (Objeto3D& objeto : objetos) {
        glDeleteVertexArrays(1, &objeto.VAO);
        glDeleteBuffers(1, &objeto.VBO);
    }

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}