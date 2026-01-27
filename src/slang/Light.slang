module Light;

interface ILight {
	float4 calc_illusion();
}

class PointLight : ILight {
	float4 calc_illusion() {}
};