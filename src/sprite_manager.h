#ifndef SPRITE_MANAGER_H
#define SPRITE_MANAGER_H

#include "basic_transform.h"
#include "glad/glad.h"
#include "texture_atlas.h"
#include "vector_util.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GL_SILENCE_DEPRECATION  // silences macOS OpenGL deprecation warnings

#define glCheck(expr)                                                                              \
    expr;                                                                                          \
    if (int e = glGetError()) {                                                                    \
        std::cerr << "OpenGL error: " << e << " in file " __FILE__ << ", line number " << __LINE__ \
                  << '\n';                                                                         \
        abort();                                                                                   \
    }

using SpriteID = size_t;

class SpriteManager {
public:
    SpriteManager(const sf::Vector2u& atlasSize, unsigned padding)
        : mVertices(sf::PrimitiveType::Triangles),
          mNumSprites{0},
          mAtlas(atlasSize, padding),
          mShader(std::filesystem::path{"main.vert"}, std::filesystem::path{"main.frag"}),
          mSFMLShader(std::filesystem::path{"main_sfml.vert"}, std::filesystem::path{"main.frag"}) {
        mSFMLShader.setUniform("texture", sf::Shader::CurrentTexture);
        const GLuint program = mShader.getNativeHandle();
        mPositionAttribLocation = glGetAttribLocation(program, "position");
        mTexCoordsAttribLocation = glGetAttribLocation(program, "texCoords");
        mSpriteNumberAttribLocation = glGetAttribLocation(program, "spriteNumber");
        mTextureUniformLocation = glGetUniformLocation(program, "texture");
        mTransformsUniformLocation = glGetUniformLocation(program, "transforms");
        mNumTransformsUniformLocation = glGetUniformLocation(program, "numTransforms");

        glGenBuffers(1, &mVerticesVBO);
        glGenBuffers(1, &mSpriteNumberVBO);
        glGenTextures(1, &mTransformsTexture);
        glBindTexture(GL_TEXTURE_1D, mTransformsTexture);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_1D, 0);
    }

    ~SpriteManager() {
        glDeleteBuffers(1, &mVerticesVBO);
        glDeleteBuffers(1, &mSpriteNumberVBO);
        glDeleteTextures(1, &mTransformsTexture);
    }

    TextureID registerTexture(const std::string& filepath) {
        return mAtlas.registerTexture(filepath);
    }

    SpriteID createSprite(TextureID textureID) {
        const SpriteID id = mNumSprites++;
        mVertices.resize(6 * mNumSprites);
        mTransforms.push_back({});
        mSpriteNumbers.resize(mVertices.getVertexCount(), static_cast<float>(id));
        setTexture(id, textureID);
        return id;
    }

    void setTexture(SpriteID sprite, TextureID newTexture) {
        assert(sprite < mNumSprites);
        const sf::Vector2f size(mAtlas.getSizePixels(newTexture));
        const sf::FloatRect uv = mAtlas.getBoundsUV(newTexture);
        const sf::Vector2f corners[] = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
        for (size_t i = 0; i < 6; ++i) {
            auto& vertex = mVertices[6 * sprite + i];
            vertex.position = corners[i] * size;
            vertex.texCoords = uv.position + corners[i] * uv.size;
        }
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        sf::VertexArray vertices = mVertices;
        for (size_t i = 0; i < vertices.getVertexCount(); ++i) {
            vertices[i].position = mTransforms[i / 6].applyPoint(vertices[i].position);
        }
        states.texture = &mAtlas.getTexture();
        states.coordinateType = sf::CoordinateType::Normalized;
        states.shader = &mSFMLShader;
        target.draw(vertices, states);
    }

    void drawGL() {
        const auto count = mVertices.getVertexCount();
        if (count == 0) {
            return;
        }
        glUseProgram(mShader.getNativeHandle());
        glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO);
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(sf::Vertex), &mVertices[0], GL_STREAM_DRAW);
        glVertexAttribPointer(mPositionAttribLocation, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex),
                              nullptr);
        glVertexAttribPointer(mTexCoordsAttribLocation, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex),
                              (const void*)(sizeof(sf::Vector2f) + sizeof(sf::Color)));
        glEnableVertexAttribArray(mPositionAttribLocation);
        glEnableVertexAttribArray(mTexCoordsAttribLocation);

        glBindBuffer(GL_ARRAY_BUFFER, mSpriteNumberVBO);
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(float), mSpriteNumbers.data(), GL_STREAM_DRAW);
        glVertexAttribPointer(mSpriteNumberAttribLocation, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(mSpriteNumberAttribLocation);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, mTransformsTexture);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F_ARB, static_cast<GLsizei>(mNumSprites), 0,
                     GL_RGBA, GL_FLOAT, mTransforms.data());
        glUniform1i(mTransformsUniformLocation, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mAtlas.getTexture().getNativeHandle());
        glUniform1i(mTextureUniformLocation, 0);
        glUniform1i(mNumTransformsUniformLocation, static_cast<GLint>(mNumSprites));
        glCheck(glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count)));

        glDisableVertexAttribArray(mPositionAttribLocation);
        glDisableVertexAttribArray(mTexCoordsAttribLocation);
        glDisableVertexAttribArray(mSpriteNumberAttribLocation);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
    }

    BasicTransform& getTransform(SpriteID id) {
        assert(id < mNumSprites);
        return mTransforms[id];
    }

    const TextureAtlas& getAtlas() const { return mAtlas; }

private:
    sf::VertexArray mVertices;
    std::vector<BasicTransform> mTransforms;
    size_t mNumSprites;
    TextureAtlas mAtlas;
    mutable sf::Shader mShader;
    sf::Shader mSFMLShader;

    // Use the below only in Batched rendering (part 2):
    std::vector<float> mSpriteNumbers;

    GLint mTextureUniformLocation;
    GLint mTransformsUniformLocation;
    GLint mNumTransformsUniformLocation;

    GLint mPositionAttribLocation;
    GLint mTexCoordsAttribLocation;
    GLint mSpriteNumberAttribLocation;

    GLuint mVerticesVBO;
    GLuint mSpriteNumberVBO;
    GLuint mTransformsTexture;
};

#endif  // SPRITE_MANAGER_H
