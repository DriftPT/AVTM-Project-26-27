bool Import3DFromFile(const std::string& pFile, Assimp::Importer& importer, const aiScene*& sc, float& scaleFactor);
void createMyMeshFromAssimp(const aiScene*& sc, Renderer* render);