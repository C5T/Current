// curl localhost:3000/$(curl -s --data-binary @example2.inl localhost:3000 | cut -f2 -d@)

// Just make sure to access every flower.
double sum[4] = { 0, 0, 0, 0 };
for (auto const& flower : flowers) {
  for (size_t i = 0; i < 4; ++i) {
    sum[i] += flower.x[i];
  }
}
os << "An 'average' flower:";
for (int i = 0; i < 4; ++i) {
  os << ' ' << std::fixed << std::setprecision(2) << sum[i] / flowers.size();
}
os << '\n';
