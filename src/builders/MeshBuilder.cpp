#include "MeshBuilder.hpp"

// temp
#include <util/NoiseFactory.hpp>

namespace hyperion::v2 {

const std::vector<Vertex> MeshBuilder::quad_vertices = {
    Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
};

const std::vector<Mesh::Index> MeshBuilder::quad_indices = {
    0, 3, 2,
    0, 2, 1
};

const std::vector<Vertex> MeshBuilder::cube_vertices = {
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    
    Vertex{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},

    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    
    Vertex{{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

    Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},

    Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    
    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}
};

std::unique_ptr<Mesh> MeshBuilder::Quad(Topology topology)
{
    std::unique_ptr<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

#ifndef HYP_APPLE
    switch (topology) {
    case Topology::TRIANGLE_FAN: {
        auto [new_vertices, new_indices] = Mesh::CalculateIndices(quad_vertices);

        mesh = std::make_unique<Mesh>(
            new_vertices,
            new_indices,
            topology,
            vertex_attributes
        );

        break;
    }
    default:
#endif
        mesh = std::make_unique<Mesh>(
            quad_vertices,
            quad_indices,
            topology,
            vertex_attributes
        );

#ifndef HYP_APPLE
        break;
    }
#endif

    mesh->CalculateTangents();

    return std::move(mesh);
}

std::unique_ptr<Mesh> MeshBuilder::Cube()
{
    std::unique_ptr<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

    auto mesh_data = Mesh::CalculateIndices(cube_vertices);

    mesh = std::make_unique<Mesh>(
        mesh_data.first,
        mesh_data.second,
        Topology::TRIANGLES,
        vertex_attributes
    );

    mesh->CalculateTangents();

    return std::move(mesh);
}

std::unique_ptr<Mesh> MeshBuilder::DividedQuad(UInt num_divisions)
{
    NoiseCombinator noise_combinator(123);
    noise_combinator
        .Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, 1.0f, 0.0f, Vector(1.0f, 1.0f, 0.0f, 0.0f));

    num_divisions = MathUtil::Max(num_divisions, 1);

    Vector3 start(0.0f);

    if (num_divisions == 1) {
        return Quad();
    }

    const UInt num_divisions_total = num_divisions * num_divisions;

    std::vector<Vertex> vertices;
    vertices.reserve(quad_vertices.size() * num_divisions_total);

    std::vector<Mesh::Index> indices;
    indices.reserve(quad_indices.size() * num_divisions_total);

    const Vector3 quad_scale = Vector3(1.0f / static_cast<Float>(num_divisions * 2.0f));

    for (UInt x = 0; x < num_divisions; x++) {
        for (UInt y = 0; y < num_divisions; y++) {
            const auto vertices_size = vertices.size();

            for (auto &vert : quad_vertices) {
                auto vert_transformed = Transform(
                    Vector3(
                        static_cast<Float>(x) / static_cast<Float>(num_divisions),
                        static_cast<Float>(y) / static_cast<Float>(num_divisions),
                        0.0f
                    ),
                    quad_scale
                    // Quaternion(Vector3(1.0f), )
                ) * vert;

                vert_transformed.SetPosition(vert_transformed.GetPosition() + vert_transformed.GetNormal() * noise_combinator.GetNoise(Vector2(vert_transformed.GetPosition())));

                vertices.push_back(vert_transformed);
            }

            for (auto idx : quad_indices) {
                indices.push_back(idx + vertices_size);
            }
        }
    }

    auto mesh = std::make_unique<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
    );

    mesh->CalculateNormals();
    return ApplyTransform(mesh.get(), Transform(Vector3(), Vector3::One(), Quaternion(
        Quaternion(Vector3(
            90.0f,
            0.0f,
            0.0f
        ))
    )));
}

std::unique_ptr<Mesh> MeshBuilder::NormalizedCubeSphere(UInt num_divisions)
{
    const Float step = 1.0f / static_cast<Float>(num_divisions);

    static const Vector3 origins[6] = {
        Vector3(-1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, -1.0f, 1.0f)
    };

    static const Vector3 rights[6] = {
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(-2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, -2.0f),
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(2.0f, 0.0f, 0.0f)
    };

    static const Vector3 ups[6] = {
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(0.0f, 0.0f, -2.0f)
    };

    std::vector<Vertex> vertices;
    std::vector<Mesh::Index> indices;

    for (UInt face = 0; face < 6; face++) {
        const Vector3 &origin = origins[face];
        const Vector3 &right = rights[face];
        const Vector3 &up = ups[face];

        for (UInt j = 0; j < num_divisions + 1; j++) {
            for (UInt i = 0; i < num_divisions + 1; i++) {
                const Vector3 point = (origin + Vector3(step) * (Vector3(i) * right + Vector3(j) * up)).Normalized();
                Vector3 position = point;
                Vector3 normal = point;

                const Vector2 uv(
                    static_cast<Float>(j + (face * num_divisions)) / static_cast<Float>(num_divisions * 6),
                    static_cast<Float>(i + (face * num_divisions)) / static_cast<Float>(num_divisions * 6)
                );

                vertices.push_back(Vertex(position, uv));
            }
        }
    }

    const UInt k = num_divisions + 1;

    for (UInt face = 0; face < 6; face++) {
        for (UInt j = 0; j < num_divisions; j++) {
            const bool is_bottom = j < (num_divisions / 2);

            for (UInt i = 0; i < num_divisions; i++) {
                const bool is_left = i < (num_divisions / 2);

                const UInt a = (face * k + j) * k + i;
                const UInt b = (face * k + j) * k + i + 1;
                const UInt c = (face * k + j + 1) * k + i;
                const UInt d = (face * k + j + 1) * k + i + 1;

                if (is_bottom ^ is_left) {
                    indices.push_back(a);
                    indices.push_back(c);
                    indices.push_back(b);
                    indices.push_back(c);
                    indices.push_back(d);
                    indices.push_back(b);
                } else {
                    indices.push_back(a);
                    indices.push_back(c);
                    indices.push_back(d);
                    indices.push_back(a);
                    indices.push_back(d);
                    indices.push_back(b);
                }
            }
        }
    }

    auto mesh = std::make_unique<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
    );

    mesh->CalculateNormals(true);
    mesh->CalculateTangents();

    return mesh;
}

std::unique_ptr<Mesh> MeshBuilder::ApplyTransform(const Mesh *mesh, const Transform &transform)
{
    AssertThrow(mesh != nullptr);

    std::vector<Vertex> new_vertices(mesh->GetVertices());

    const auto normal_matrix = transform.GetMatrix().Inverted().Transposed();

    for (Vertex &vertex : new_vertices) {
        vertex.SetPosition(transform.GetMatrix() * vertex.GetPosition());
        vertex.SetNormal(normal_matrix * vertex.GetNormal());
        vertex.SetTangent(normal_matrix * vertex.GetTangent());
        vertex.SetBitangent(normal_matrix * vertex.GetBitangent());
    }

    return std::make_unique<Mesh>(
        new_vertices,
        mesh->GetIndices(),
        mesh->GetTopology(),
        mesh->GetVertexAttributes(),
        mesh->GetFlags()
    );
}

std::unique_ptr<Mesh> MeshBuilder::Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform)
{
    AssertThrow(a != nullptr);
    AssertThrow(b != nullptr);

    std::unique_ptr<Mesh> transformed_meshes[2] = { ApplyTransform(a, a_transform), ApplyTransform(b, b_transform) };
    
    const auto merged_vertex_attributes = a->GetVertexAttributes() | b->GetVertexAttributes();

    std::vector<Vertex> all_vertices(transformed_meshes[0]->GetVertices().size() + transformed_meshes[1]->GetVertices().size());
    std::vector<Mesh::Index> all_indices(transformed_meshes[0]->GetIndices().size() + transformed_meshes[1]->GetIndices().size());

    SizeType vertex_offset = 0,
             index_offset = 0;

    for (SizeType mesh_index = 0; mesh_index < 2; mesh_index++) {
        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetVertices().size(); i++) {
            all_vertices[vertex_offset++] = transformed_meshes[mesh_index]->GetVertices()[i];
        }

        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetIndices().size(); i++) {
            all_indices[index_offset++] = transformed_meshes[mesh_index]->GetIndices()[i];
        }
    }

    return std::make_unique<Mesh>(
        all_vertices,
        all_indices,
        a->GetTopology(),
        merged_vertex_attributes,
        a->GetFlags()
    );
}

std::unique_ptr<Mesh> MeshBuilder::Merge(const Mesh *a, const Mesh *b)
{
    return Merge(a, b, Transform(), Transform());
}

} // namespace hyperion::v2
