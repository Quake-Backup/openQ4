#version 110

uniform vec4 uViewOrigin;
uniform vec4 uColor;
uniform vec4 uRimParams;

varying vec3 vWorldNormal;
varying vec3 vWorldPosition;

void main() {
	vec3 normal = normalize( vWorldNormal );
	vec3 viewDir = normalize( uViewOrigin.xyz - vWorldPosition );
	float rim = 1.0 - max( dot( normal, viewDir ), 0.0 );
	rim = pow( max( rim, 0.0 ), max( uRimParams.x, 0.001 ) );

	float contribution = uColor.a * rim * uRimParams.y + uRimParams.z;
	contribution = clamp( contribution, 0.0, 1.0 );
	gl_FragColor = vec4( uColor.rgb * contribution, contribution );
}
