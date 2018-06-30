// To run: make

#include "current.h"

#include "current/bricks/graph/gnuplot.h"

template <typename T>
void Validate(T& x, T min, T max) {
  if (x < min) {
    x = min;
  } else if (x > max) {
    x = max;
  }
}

void Smoothen(std::vector<double>& v) {
  std::vector<double> v2 = v;
  for (size_t i = 2; i + 2 < v.size(); ++i) {
    v2[i] = (v[i] * 6 + (v[i-1] + v[i+1]) * 4 + (v[i-2]+v[i+2])) / (1+4+6+4+1);
  }
  v = v2;
}

ENDPOINT(data, params) {
  const int buckets = 1000;

  const double min_speed = 3;
  const double max_speed = 40;

  const double k = 1.0 * buckets / (max_speed - min_speed);

  auto from_hour = current::FromString<int>(params["from"]);
  auto to_hour = params.count("to") ? current::FromString<int>(params["to"]) : 24;
  Validate(from_hour, 0, 23);
  Validate(to_hour, from_hour + 1, 24);

  std::vector<std::vector<double>> histogram(24, std::vector<double>(buckets, 0));

  for (const auto& ride : data) {
    const int trip_duration_seconds = ride.dropoff.epoch - ride.pickup.epoch;
    if (ride.trip_distance_times_100 > 50 && trip_duration_seconds > 300 && trip_duration_seconds < 3600 * 2) {
      const double cost_per_mile = 1.0 * ride.fare_amount_cents / ride.trip_distance_times_100;
      if (cost_per_mile >= 2 && cost_per_mile <= 10) {
        const double ride_speed = (0.01 * ride.trip_distance_times_100) / (trip_duration_seconds * (1.0 / 3600));
        if (ride_speed >= min_speed && ride_speed <= max_speed) {
          int bucket = (ride_speed - min_speed) * k;
          histogram[ride.pickup.hour][bucket >= buckets ? buckets - 1 : bucket] += buckets;
        }
      }
    }
  }

  for (int q = 0; q < 50; ++q) {
    for (int i = 0; i < 24; ++i) {
      Smoothen(histogram[i]);
    }
  }

  auto plot = current::gnuplot::GNUPlot(); 

  plot.Title("Rides per hour average speed histogram, smoothened.").XLabel("MPH").YLabel("Rides");
  plot.KeyTitle("Hour of day").Grid("back");

  for (int hour = from_hour; hour < to_hour; ++hour) {
    plot.Plot(current::gnuplot::WithMeta([hour, buckets, min_speed, max_speed, &histogram](current::gnuplot::Plotter& p) {
      p(min_speed, 0);
      for (int i = 0; i < buckets; ++i) {
        double x1 = min_speed + (max_speed - min_speed) * i / buckets;
        double x2 = min_speed + (max_speed - min_speed) * (i + 1) / buckets;
        p(x1, histogram[hour][i]);
        p(x2, histogram[hour][i]);
      }
      p(max_speed, 0);
    }).LineWidth(2).Name(Printf("%02d", hour)));
  }
  
  std::string svg = plot.ImageSize(400).OutputFormat("svg");

  return Response(svg).ContentType("image/svg+xml");
}
