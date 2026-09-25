#version 430

struct Materials {
	vec4 diffuse;
	vec4 ambient;
	vec4 specular;
	vec4 emissive;
	float shininess;
	int texCount;
};

in Data {
	vec3 normal;
	vec3 eye;
	vec3 lightDir;
	vec2 tex_coord;
	vec3 fragPos;
} DataIn;

uniform Materials mat;

uniform sampler2D texmap;
uniform sampler2D texmap1;
uniform sampler2D texmap2;

uniform int texMode;

// Global Illumination - Directional Light
uniform bool globalLightOn;
uniform vec4 globalLightPos;

// Spotlight Illumination
uniform bool spotlight_mode;
uniform vec4 spotLightPos0;
uniform vec4 spotLightPos1;
uniform vec4 coneDir;
uniform float spotCosCutOff;

// 6 Candles Lights - Point Lights
uniform bool candlesLightOn;
uniform vec4 candlesLightPos0, candlesLightPos1, candlesLightPos2, candlesLightPos3, candlesLightPos4, candlesLightPos5;

uniform bool fogEnabled;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform int depthFog;

out vec4 colorOut;

// Calculate the global illumination
vec3 calculateGlobalLight(vec3 n, vec3 e,out vec3 specular)
{
    if (!globalLightOn)
        return vec3(0.0);

    vec3 l = normalize(globalLightPos.xyz);

	// Ambient
    vec3 ambient = mat.ambient.rgb;

	// Diffuse
    float intensity = max(dot(n, l), 0.0);
    vec3 diffuse = intensity * mat.diffuse.rgb;

	specular = vec3(0.0);

    if (intensity > 0.0) {
        vec3 h = normalize(l+e);
        float intSpec = max(dot(h,n), 0.0);
        specular = mat.specular.rgb * pow(intSpec, mat.shininess);
    }

    return ambient + diffuse;
}

//Calculate the spotlight illumination
vec3 calculateSpotLight(vec4 lightPos, vec3 spotDirection, vec3 n, vec3 e){
	vec4 spec = vec4(0.0);
	float intensity = 0.0f;
	float intSpec = 0.0f;

	float att = 0.0;
	float spotExp = 60.0;

	if (!spotlight_mode) //spotlight on?
		return vec3(0.0);

	// Vector from fragment to light
	vec3 lightVector = lightPos.xyz - DataIn.fragPos;
    float distanceToLight = length(lightVector);

	// Direction from fragment to light
	vec3 l = normalize(lightVector);
	vec3 sd = normalize(spotDirection);
	
	float spotCos = dot(-l, sd);
	if(spotCos <= spotCosCutOff) // inside the cone?
		return vec3(0.0);

	att = pow(spotCos, spotExp);
	intensity = max(dot(n,l), 0.0) * att;

	if (intensity <= 0.0)
        return vec3(0.0);

	// Calculate diffuse component
	vec3 diffuse = intensity * mat.diffuse.rgb;

	vec3 h = normalize(l + e);
	intSpec = max(dot(h, n), 0.0);
	spec = mat.specular * pow(intSpec, mat.shininess) * att;

	return diffuse + spec.rgb;
}

// Calculate the candles illumination
vec3 calculateCandleLight(vec4 lightPos, vec3 n, vec3 e){
	vec4 spec = vec4(0.0);
	float intensity = 0.0f;
	float intSpec = 0.0f;

    // Vector from fragment to candle
    vec3 lightVector = lightPos.xyz - DataIn.fragPos;
    float distanceToLight =  length(lightVector);

    // Direction to candle
    vec3 l = normalize(lightVector);

    // Diffuse
	intensity = max(dot(n, l), 0.0);
    if (intensity <= 0.0)
        return vec3(0.0);
    vec3 diffuse = intensity * mat.diffuse.rgb;

    // Specular
    vec3 h = normalize(l + e);
    intSpec = max(dot(h, n), 0.0);
    vec3 specular = mat.specular.rgb * pow(intSpec, mat.shininess);

	// Final Color
    vec3 result = diffuse + specular;
    return result;
}

void main() {
	vec4 texel, texel1;

	vec3 n = normalize(DataIn.normal);
	vec3 e = normalize(DataIn.eye);

	//Global Light
	vec3 globalSpecular; 
	vec3 globalLight = calculateGlobalLight(n, e, globalSpecular);

	// Spotlight
	vec3 spotlight = vec3(0.0);

	if (spotlight_mode)	{
		spotlight += calculateSpotLight(spotLightPos0, coneDir.xyz, n, e);
		spotlight += calculateSpotLight(spotLightPos1, coneDir.xyz, n, e);
	}

	// Candle Lights
	vec3 candleLights = vec3(0.0);
	if(candlesLightOn){
		candleLights += calculateCandleLight(candlesLightPos0, n, e);
		candleLights += calculateCandleLight(candlesLightPos1, n, e);
		candleLights += calculateCandleLight(candlesLightPos2, n, e);
		candleLights += calculateCandleLight(candlesLightPos3, n, e);
		candleLights += calculateCandleLight(candlesLightPos4, n, e);
		candleLights += calculateCandleLight(candlesLightPos5, n, e);
	}

	// Final Light
	vec3 baseColor;
	vec3 diffuseLight = globalLight + spotlight + candleLights;

	// Textures
	if(texMode == 0) //no texturing
		baseColor = diffuseLight + mat.ambient.rgb;

	else if(texMode == 1) // modulate diffuse color with texel color
	{
		texel = texture(texmap2, DataIn.tex_coord);  // texel from lighwood.tga
		baseColor = max(diffuseLight * texel.rgb, 0.07 * texel.rgb);
	}
	else if (texMode == 2) // diffuse color is replaced by texel color
	{
		texel = texture(texmap, DataIn.tex_coord);  // texel from stone.tga
		baseColor = max(diffuseLight * texel.rgb, 0.07 * texel.rgb);
	}
	else // multitexturing
	{
		texel = texture(texmap2, DataIn.tex_coord);  // texel from lighwood.tga
		texel1 = texture(texmap1, DataIn.tex_coord);  // texel from checker.tga
		baseColor = max(diffuseLight * texel.rgb * texel1.rgb, 0.07 * texel.rgb * texel1.rgb);
	}

	vec3 finalColor = baseColor + globalSpecular; 
	colorOut = vec4(finalColor, mat.diffuse.a);

	if (fogEnabled) {
		float dist;

		if (depthFog == 0)
		{
			// Plane based
			dist = abs(DataIn.eye.z);
		}
		else
		{
			// Range based
			dist = length(DataIn.eye);
		}

		float fogFactor = (fogEnd - dist) / (fogEnd - fogStart);
		fogFactor = clamp(fogFactor, 0.0, 1.0);

		colorOut.rgb = mix(fogColor, colorOut.rgb, fogFactor);
	}
}